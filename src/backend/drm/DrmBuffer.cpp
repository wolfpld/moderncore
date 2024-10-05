#include <drm_fourcc.h>
#include <gbm.h>
#include <sys/stat.h>
#include <tracy/Tracy.hpp>
#include <xf86drm.h>

#include "DrmBuffer.hpp"
#include "DrmDevice.hpp"
#include "backend/GpuDevice.hpp"
#include "util/Logs.hpp"
#include "util/Panic.hpp"
#include "vulkan/VlkDevice.hpp"
#include "vulkan/VlkError.hpp"
#include "vulkan/VlkProxy.hpp"

namespace
{
bool CheckDisjoint( const std::vector<int>& dmaBufFds )
{
    if( dmaBufFds.size() == 1 ) return false;

    struct stat st_ref, st;
    if( fstat( dmaBufFds[0], &st_ref ) < 0 ) return false;

    for( size_t i=1; i<dmaBufFds.size(); i++ )
    {
        if( fstat( dmaBufFds[i], &st ) < 0 ) return false;
        if( st.st_ino != st_ref.st_ino ) return true;
    }
    return false;
}
}

DrmBuffer::DrmBuffer( DrmDevice& device, const drmModeModeInfo& mode, const std::vector<uint64_t>& modifiers )
    : m_device( device )
{
    ZoneScoped;

    m_bo = gbm_bo_create_with_modifiers( device, mode.hdisplay, mode.vdisplay, DRM_FORMAT_XRGB8888, modifiers.data(), modifiers.size() );
    CheckPanic( m_bo, "Failed to create gbm buffer" );

    m_modifier = gbm_bo_get_modifier( m_bo );
    const auto numPlanes = gbm_bo_get_plane_count( m_bo );

    if( GetLogLevel() <= LogLevel::Debug )
    {
        auto modifierName = drmGetFormatModifierName( m_modifier );
        mclog( LogLevel::Debug, "Buffer modifier: 0x%llx (%s), num planes: %d", m_modifier, modifierName, numPlanes );
        free( modifierName );
    }

    std::vector<int> dmaBufFds( numPlanes );
    std::vector<uint32_t> pitches( numPlanes );
    std::vector<uint32_t> offsets( numPlanes );
    std::vector<VkSubresourceLayout> layouts( numPlanes );
    for( uint32_t i=0; i<numPlanes; i++ )
    {
        dmaBufFds[i] = gbm_bo_get_fd_for_plane( m_bo, i );

        layouts[i].offset = offsets[i] = gbm_bo_get_offset( m_bo, i );
        layouts[i].rowPitch = pitches[i] = gbm_bo_get_stride_for_plane( m_bo, i );
        layouts[i].arrayPitch = 0;
        layouts[i].depthPitch = 0;

        mclog( LogLevel::Debug, "  Plane %d: offset %d, pitch %d", i, layouts[i].offset, layouts[i].rowPitch );
    }

    std::vector<uint64_t> drmModifiers( numPlanes, m_modifier );
    CheckPanic( drmModeAddFB2WithModifiers( device.Descriptor(), mode.hdisplay, mode.vdisplay, DRM_FORMAT_XRGB8888, (uint32_t*)dmaBufFds.data(), pitches.data(), offsets.data(), drmModifiers.data(), &m_kmsFb, DRM_MODE_FB_MODIFIERS ), "Failed to create KMS framebuffer" );

    const bool disjoint = CheckDisjoint( dmaBufFds );
    CheckPanic( !disjoint, "Disjoint buffers are not supported" );

    VkImageDrmFormatModifierExplicitCreateInfoEXT modInfo = { VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT };
    modInfo.drmFormatModifier = m_modifier;
    modInfo.drmFormatModifierPlaneCount = numPlanes;
    modInfo.pPlaneLayouts = layouts.data();

    VkExternalMemoryImageCreateInfo extInfo = { VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO };
    extInfo.pNext = &modInfo;
    extInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;

    VkImageCreateInfo imageInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    imageInfo.pNext = &extInfo;
    imageInfo.flags = disjoint ? VK_IMAGE_CREATE_DISJOINT_BIT : 0;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_B8G8R8A8_SRGB;
    imageInfo.extent.width = mode.hdisplay;
    imageInfo.extent.height = mode.vdisplay;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
    imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    auto& dev = *device.Gpu()->Device();
    VkVerify( vkCreateImage( dev, &imageInfo, nullptr, &m_image ) );

    VkMemoryFdPropertiesKHR memFdProps = { VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR };
    VkVerify( GetMemoryFdPropertiesKHR( dev, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, dmaBufFds[0], &memFdProps ) );

    VkImageMemoryRequirementsInfo2 memReqInfo = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2 };
    memReqInfo.image = m_image;

    VkMemoryRequirements2 memReq = { VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2 };

    vkGetImageMemoryRequirements2( dev, &memReqInfo, &memReq );
    mclog( LogLevel::Debug, "  Memory requirements: size %d, alignment %d", memReq.memoryRequirements.size, memReq.memoryRequirements.alignment );

    auto memIdx = FindMemoryType( memReq.memoryRequirements.memoryTypeBits & memFdProps.memoryTypeBits, 0 );
    CheckPanic( memIdx >= 0, "Failed to find suitable memory type" );

    VkMemoryDedicatedAllocateInfo dedInfo = { VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO };
    dedInfo.image = m_image;

    VkImportMemoryFdInfoKHR importInfo = { VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR };
    importInfo.pNext = &dedInfo;
    importInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
    importInfo.fd = dmaBufFds[0];

    dmaBufFds[0] = -1;      // ownership transferred

    VkMemoryAllocateInfo memInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    memInfo.pNext = &importInfo;
    memInfo.allocationSize = memReq.memoryRequirements.size;
    memInfo.memoryTypeIndex = memIdx;

    VkVerify( vkAllocateMemory( dev, &memInfo, nullptr, &m_memory ) );

    VkBindImageMemoryInfo bindInfo = { VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO };
    bindInfo.image = m_image;
    bindInfo.memory = m_memory;
    bindInfo.memoryOffset = 0;

    VkVerify( vkBindImageMemory2( dev, 1, &bindInfo ) );

    for( auto& fd : dmaBufFds )
    {
        if( fd != -1 ) close( fd );
    }
}

DrmBuffer::~DrmBuffer()
{
    auto& dev = *m_device.Gpu()->Device();
    vkFreeMemory( dev, m_memory, nullptr );
    vkDestroyImage( dev, m_image, nullptr );
    drmModeRmFB( m_device.Descriptor(), m_kmsFb );
    gbm_bo_destroy( m_bo );
}

int DrmBuffer::FindMemoryType( uint32_t typeBits, VkMemoryPropertyFlags properties )
{
    const auto& memProps = m_device.Gpu()->Device()->GetPhysicalDevice()->MemoryProperties();
    for( uint32_t i=0; i<memProps.memoryTypeCount; i++ )
    {
        if( (typeBits & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & properties) == properties )
        {
            return i;
        }
    }
    return -1;
}
