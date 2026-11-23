#include <algorithm>
#include <array>

#include "MipGen.hpp"

#include "ImageSync.hpp"
#include "shader/MipGenComp.hpp"
#include "util/EmbedData.hpp"
#include "util/Panic.hpp"
#include "vulkan/VlkBase.hpp"
#include "vulkan/VlkDescriptorSetLayout.hpp"
#include "vulkan/VlkDevice.hpp"
#include "vulkan/VlkImageView.hpp"
#include "vulkan/VlkPipeline.hpp"
#include "vulkan/VlkPipelineLayout.hpp"
#include "vulkan/VlkSampler.hpp"
#include "vulkan/VlkShaderModule.hpp"

namespace
{
struct MipPush
{
    uint32_t srcW;
    uint32_t srcH;
    uint32_t dstW;
    uint32_t dstH;
    uint32_t encodeSrgb;
};

constexpr uint32_t maxGpuMipDimension = 16384;

[[nodiscard]] VkImageViewCreateInfo GetLevelViewInfo( VkImage image, VkFormat format, uint32_t level,
                                                      const VkImageViewUsageCreateInfo& usageInfo )
{
    return {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = &usageInfo,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, level, 1, 0, 1 }
    };
}

[[nodiscard]] uint32_t DivCeil( uint32_t value, uint32_t divisor )
{
    return ( value + divisor - 1 ) / divisor;
}
}

MipGen::MipGen( VlkDevice& device )
    : m_device( device )
    , m_computeFamily( uint32_t( device.GetQueueInfo( QueueType::Compute ).idx ) )
{
    Unembed( MipGenComp );
    m_module = std::make_unique<VlkShaderModule>( device, *MipGenComp );

    const VkSamplerCreateInfo samplerInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .maxLod = 0.0f
    };
    m_sampler = std::make_unique<VlkSampler>( device, samplerInfo );

    const std::array<VkDescriptorSetLayoutBinding, 2> bindings = { {
        { .binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT },
        { .binding = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT }
    } };
    const VkDescriptorSetLayoutCreateInfo setLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,
        .bindingCount = uint32_t( bindings.size() ),
        .pBindings = bindings.data()
    };
    m_setLayout = std::make_unique<VlkDescriptorSetLayout>( device, setLayoutInfo );

    const std::array<VkDescriptorSetLayout, 1> sets = { *m_setLayout };
    const VkPushConstantRange pushRange = {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .offset = 0,
        .size = sizeof( MipPush )
    };
    const VkPipelineLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = uint32_t( sets.size() ),
        .pSetLayouts = sets.data(),
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushRange
    };
    m_pipelineLayout = std::make_unique<VlkPipelineLayout>( device, layoutInfo );

    const VkComputePipelineCreateInfo pipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = *m_module,
            .pName = "main"
        },
        .layout = *m_pipelineLayout
    };
    m_pipeline = std::make_unique<VlkPipeline>( device, pipelineInfo );
}

bool MipGen::Supports( VkFormat format, uint32_t width, uint32_t height ) const
{
    if( m_device.GetQueueInfo( QueueType::Compute ).idx < 0 ) return false;
    if( width > maxGpuMipDimension || height > maxGpuMipDimension ) return false;

    const VkFormat storageFormat = StorageViewFormat( format );
    VkFormatProperties fp = {};
    vkGetPhysicalDeviceFormatProperties( *m_device.GetPhysicalDevice(), format, &fp );
    if( ( fp.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT ) == 0 ) return false;

    if( storageFormat != format )
    {
        vkGetPhysicalDeviceFormatProperties( *m_device.GetPhysicalDevice(), storageFormat, &fp );
    }
    return ( fp.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT ) != 0;
}

void MipGen::GenerateChain( VkCommandBuffer cmdBuf, VkImage image, VkFormat format,
    uint32_t width, uint32_t height, uint32_t mipLevels,
    uint32_t sourceFamily, uint32_t releaseFamily,
    std::vector<std::shared_ptr<VlkBase>>& keepAlive )
{
    CheckPanic( mipLevels > 1, "Mip generation chain requires more than one level" );

    if( sourceFamily != VK_QUEUE_FAMILY_IGNORED )
    {
        ImageSync::Barrier( cmdBuf, image,
            VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
            sourceFamily, m_computeFamily, 0, 1 );
    }

    vkCmdBindPipeline( cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, *m_pipeline );

    const uint32_t encodeSrgb = format == VK_FORMAT_R8G8B8A8_SRGB ? 1u : 0u;
    const VkFormat storageFormat = StorageViewFormat( format );
    const VkImageViewUsageCreateInfo srcUsage = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT
    };
    const VkImageViewUsageCreateInfo dstUsage = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO,
        .usage = VK_IMAGE_USAGE_STORAGE_BIT
    };

    uint32_t w = width;
    uint32_t h = height;
    for( uint32_t level = 1; level < mipLevels; level++ )
    {
        const uint32_t dw = std::max( 1u, w / 2 );
        const uint32_t dh = std::max( 1u, h / 2 );

        const auto srcView = std::make_shared<VlkImageView>( m_device, GetLevelViewInfo( image, format, level - 1, srcUsage ) );
        const auto dstView = std::make_shared<VlkImageView>( m_device, GetLevelViewInfo( image, storageFormat, level, dstUsage ) );
        keepAlive.emplace_back( srcView );
        keepAlive.emplace_back( dstView );

        ImageSync::Barrier( cmdBuf, image,
            VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_ACCESS_2_NONE,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
            VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, level, 1 );

        const VkDescriptorImageInfo srcInfo = {
            .sampler = *m_sampler,
            .imageView = *srcView,
            .imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL
        };
        const VkDescriptorImageInfo dstInfo = {
            .imageView = *dstView,
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL
        };
        const std::array<VkWriteDescriptorSet, 2> writes = { {
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstBinding = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = &srcInfo
            },
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstBinding = 1,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .pImageInfo = &dstInfo
            }
        } };
        vkCmdPushDescriptorSet( cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, *m_pipelineLayout, 0,
            uint32_t( writes.size() ), writes.data() );

        const MipPush push = { .srcW = w, .srcH = h, .dstW = dw, .dstH = dh, .encodeSrgb = encodeSrgb };
        vkCmdPushConstants( cmdBuf, *m_pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( push ), &push );
        vkCmdDispatch( cmdBuf, DivCeil( dw, 8 ), DivCeil( dh, 8 ), 1 );

        ImageSync::Barrier( cmdBuf, image,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
            VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, level, 1 );

        w = dw;
        h = dh;
    }

    if( releaseFamily != VK_QUEUE_FAMILY_IGNORED )
    {
        ImageSync::Barrier( cmdBuf, image,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
            0, 0,
            VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
            m_computeFamily, releaseFamily, 0, mipLevels );
    }
}
