#include <algorithm>
#include <cmath>
#include <string.h>
#include <vector>

#include "ImageView.hpp"
#include "util/Bitmap.hpp"
#include "util/BitmapHdr.hpp"
#include "util/EmbedData.hpp"
#include "vulkan/VlkBuffer.hpp"
#include "vulkan/VlkCommandBuffer.hpp"
#include "vulkan/VlkDescriptorSetLayout.hpp"
#include "vulkan/VlkDevice.hpp"
#include "vulkan/VlkFence.hpp"
#include "vulkan/VlkPipeline.hpp"
#include "vulkan/VlkPipelineLayout.hpp"
#include "vulkan/VlkProxy.hpp"
#include "vulkan/VlkSampler.hpp"
#include "vulkan/VlkShader.hpp"
#include "vulkan/VlkShaderModule.hpp"
#include "vulkan/ext/GarbageChute.hpp"
#include "vulkan/ext/Texture.hpp"
#include "vulkan/ext/Tracy.hpp"

#include "shader/SupersampleFrag.hpp"
#include "shader/SupersamplePqFrag.hpp"
#include "shader/TexturingAlphaFrag.hpp"
#include "shader/TexturingAlphaPqFrag.hpp"
#include "shader/TexturingVert.hpp"

struct PushConstant
{
    float screenSize[2];
    float div;
};

ImageView::ImageView( GarbageChute& garbage, std::shared_ptr<VlkDevice> device, VkFormat format, const VkExtent2D& extent, float scale )
    : m_garbage( garbage )
    , m_device( std::move( device ) )
    , m_extent( extent )
    , m_scale( scale )
    , m_fitToResize( true )
{
    SetScale( scale, extent );

    constexpr VkSamplerCreateInfo samplerInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .mipLodBias = -1.f,
        .maxLod = VK_LOD_CLAMP_NONE
    };
    m_sampler = std::make_unique<VlkSampler>( *m_device, samplerInfo );

    m_imageInfo = {
        .sampler = *m_sampler,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    m_descWrite = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &m_imageInfo
    };


    Unembed( SupersampleFrag );
    Unembed( SupersamplePqFrag );
    Unembed( TexturingAlphaFrag );
    Unembed( TexturingAlphaPqFrag );
    Unembed( TexturingVert );

    auto SupersampleFragModule = std::make_shared<VlkShaderModule>( *m_device, *SupersampleFrag );
    auto SupersamplePqFragModule = std::make_shared<VlkShaderModule>( *m_device, *SupersamplePqFrag );
    auto TexturingAlphaFragModule = std::make_shared<VlkShaderModule>( *m_device, *TexturingAlphaFrag );
    auto TexturingAlphaPqFragModule = std::make_shared<VlkShaderModule>( *m_device, *TexturingAlphaPqFrag );
    auto TexturingVertModule = std::make_shared<VlkShaderModule>( *m_device, *TexturingVert );

    m_shaderMin[0] = std::make_shared<VlkShader>( std::array {
        VlkShader::Stage { TexturingVertModule, VK_SHADER_STAGE_VERTEX_BIT },
        VlkShader::Stage { SupersampleFragModule, VK_SHADER_STAGE_FRAGMENT_BIT }
    } );
    m_shaderMin[1] = std::make_shared<VlkShader>( std::array {
        VlkShader::Stage { TexturingVertModule, VK_SHADER_STAGE_VERTEX_BIT },
        VlkShader::Stage { SupersamplePqFragModule, VK_SHADER_STAGE_FRAGMENT_BIT }
    } );

    m_shaderExact[0] = std::make_shared<VlkShader>( std::array {
        VlkShader::Stage { TexturingVertModule, VK_SHADER_STAGE_VERTEX_BIT },
        VlkShader::Stage { TexturingAlphaFragModule, VK_SHADER_STAGE_FRAGMENT_BIT }
    } );
    m_shaderExact[1] = std::make_shared<VlkShader>( std::array {
        VlkShader::Stage { TexturingVertModule, VK_SHADER_STAGE_VERTEX_BIT },
        VlkShader::Stage { TexturingAlphaPqFragModule, VK_SHADER_STAGE_FRAGMENT_BIT }
    } );


    static constexpr std::array bindings = {
        VkDescriptorSetLayoutBinding { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT }
    };
    constexpr VkDescriptorSetLayoutCreateInfo setLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,
        .bindingCount = bindings.size(),
        .pBindings = bindings.data()
    };
    m_setLayout = std::make_shared<VlkDescriptorSetLayout>( *m_device, setLayoutInfo );


    const std::array<VkDescriptorSetLayout, 1> sets = { *m_setLayout };
    static constexpr std::array pushConstantRange = {
        VkPushConstantRange {
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .offset = offsetof( PushConstant, screenSize ),
            .size = sizeof( float ) * 2
        },
        VkPushConstantRange {
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset = offsetof( PushConstant, div ),
            .size = sizeof( float )
        }
    };
    const VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = sets.size(),
        .pSetLayouts = sets.data(),
        .pushConstantRangeCount = pushConstantRange.size(),
        .pPushConstantRanges = pushConstantRange.data()
    };
    m_pipelineLayout = std::make_shared<VlkPipelineLayout>( *m_device, pipelineLayoutInfo );
    CreatePipeline( format );


    constexpr uint16_t idata[] = { 0, 1, 2, 2, 3, 0 };
    constexpr VkBufferCreateInfo iinfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof( idata ),
        .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    m_indexBuffer = std::make_shared<VlkBuffer>( *m_device, iinfo, VlkBuffer::PreferDevice | VlkBuffer::WillWrite );
    memcpy( m_indexBuffer->Ptr(), idata, sizeof( idata ) );
    m_indexBuffer->Flush();
}

ImageView::~ImageView()
{
    m_garbage.Recycle( {
        std::move( m_pipelineMin ),
        std::move( m_pipelineExact ),
        std::move( m_pipelineLayout ),
        std::move( m_setLayout ),
        std::move( m_shaderMin[0] ),
        std::move( m_shaderMin[1] ),
        std::move( m_shaderExact[0] ),
        std::move( m_shaderExact[1] ),
        std::move( m_vertexBuffer ),
        std::move( m_indexBuffer ),
        std::move( m_texture ),
        std::move( m_sampler )
    } );
}

void ImageView::Render( VlkCommandBuffer& cmdbuf, const VkExtent2D& extent )
{
    CheckPanic( m_texture, "No texture" );

    const VkViewport viewport = {
        .width = float( extent.width ),
        .height = float( extent.height )
    };
    const VkRect2D scissor = {
        .extent = extent
    };

    const PushConstant pushConstant = {
        float( extent.width ),
        float( extent.height ),
        m_div
    };

    const std::array<VkBuffer, 1> vertexBuffers = { *m_vertexBuffer };
    constexpr std::array<VkDeviceSize, 1> offsets = { 0 };

    std::lock_guard lock( m_lock );

    ZoneVk( *m_device, cmdbuf, "ImageView", true );
    if( m_bitmapExtent.width <= extent.width && m_bitmapExtent.height <= extent.height )
    {
        vkCmdBindPipeline( cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineExact );
    }
    else
    {
        vkCmdBindPipeline( cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineMin );
    }
    vkCmdPushConstants( cmdbuf, *m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, offsetof( PushConstant, screenSize ), sizeof( float ) * 2, &pushConstant );
    vkCmdPushConstants( cmdbuf, *m_pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, offsetof( PushConstant, div ), sizeof( float ), &pushConstant.div );
    vkCmdSetViewport( cmdbuf, 0, 1, &viewport );
    vkCmdSetScissor( cmdbuf, 0, 1, &scissor );
    vkCmdBindVertexBuffers( cmdbuf, 0, 1, vertexBuffers.data(), offsets.data() );
    vkCmdBindIndexBuffer( cmdbuf, *m_indexBuffer, 0, VK_INDEX_TYPE_UINT16 );
    vkCmdPushDescriptorSet( cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineLayout, 0, 1, &m_descWrite );
    vkCmdDrawIndexed( cmdbuf, 6, 1, 0, 0, 0 );
}

void ImageView::Resize( const VkExtent2D& extent )
{
    std::lock_guard lock( m_lock );
    const auto dx = int32_t( extent.width - m_extent.width );
    const auto dy = int32_t( extent.height - m_extent.height );
    if( dx == 0 && dy == 0 ) return;

    if( m_fitToResize )
    {
        FitToExtent( extent );
    }
    else
    {
        m_extent = extent;
        m_imgOrigin.x += dx / 2.f;
        m_imgOrigin.y += dy / 2.f;
    }

    UpdateVertexBuffer();
}

void ImageView::SetBitmap( const std::shared_ptr<Bitmap>& bitmap, TaskDispatch& td )
{
    if( !bitmap )
    {
        std::lock_guard lock( m_lock );
        Cleanup();
        return;
    }

    std::vector<std::shared_ptr<VlkFence>> texFences;
    auto texture = std::make_shared<Texture>( *m_device, *bitmap, VK_FORMAT_R8G8B8A8_SRGB, true, texFences, &td );

    constexpr VkBufferCreateInfo vinfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof( Vertex ) * 4,
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    auto vb = std::make_shared<VlkBuffer>( *m_device, vinfo, VlkBuffer::PreferDevice | VlkBuffer::WillWrite );

    for( auto& fence : texFences ) fence->Wait();
    FinishSetBitmap( std::move( texture ), std::move( vb ), bitmap->Width(), bitmap->Height() );
}

void ImageView::SetBitmap( const std::shared_ptr<BitmapHdr>& bitmap, TaskDispatch& td )
{
    if( !bitmap )
    {
        std::lock_guard lock( m_lock );
        Cleanup();
        return;
    }

    std::vector<std::shared_ptr<VlkFence>> texFences;
    auto texture = std::make_shared<Texture>( *m_device, *bitmap, VK_FORMAT_R16G16B16A16_SFLOAT, true, texFences, &td );

    constexpr VkBufferCreateInfo vinfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof( Vertex ) * 4,
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    auto vb = std::make_shared<VlkBuffer>( *m_device, vinfo, VlkBuffer::PreferDevice | VlkBuffer::WillWrite );

    for( auto& fence : texFences ) fence->Wait();
    FinishSetBitmap( std::move( texture ), std::move( vb ), bitmap->Width(), bitmap->Height() );
}

void ImageView::FinishSetBitmap( std::shared_ptr<Texture>&& texture, std::shared_ptr<VlkBuffer>&& vb, uint32_t width, uint32_t height )
{
    std::lock_guard lock( m_lock );
    Cleanup();

    std::swap( m_texture, texture );
    m_imageInfo.imageView = *m_texture;

    m_bitmapExtent = { width, height };
    FitToExtent( m_extent );
}

void ImageView::SetScale( float scale, const VkExtent2D& extent )
{
    const auto ratio = scale / m_scale;
    m_scale = scale;
    m_extent = extent;
    m_div = 1.f / 8.f / scale;
    if( !HasBitmap() ) return;

    m_imgOrigin.x *= ratio;
    m_imgOrigin.y *= ratio;
    m_imgScale *= ratio;
    UpdateVertexBuffer();
}

void ImageView::FormatChange( VkFormat format )
{
    m_garbage.Recycle( {
        std::move( m_pipelineMin ),
        std::move( m_pipelineExact )
    } );
    CreatePipeline( format );
}

void ImageView::FitToExtent( const VkExtent2D& extent )
{
    m_fitToResize = true;
    m_extent = extent;

    if( m_bitmapExtent.width <= extent.width &&
        m_bitmapExtent.height <= extent.height )
    {
        m_imgOrigin = {
            float( extent.width - m_bitmapExtent.width ) / 2,
            float( extent.height - m_bitmapExtent.height ) / 2
        };
        m_imgScale = 1;
    }
    else
    {
        const auto ratioWidth = float( extent.width ) / m_bitmapExtent.width;
        const auto ratioHeight = float( extent.height ) / m_bitmapExtent.height;
        const auto scale = std::min( ratioWidth, ratioHeight );
        m_imgOrigin = {
            float( extent.width - m_bitmapExtent.width * scale ) / 2,
            float( extent.height - m_bitmapExtent.height * scale ) / 2
        };
        m_imgScale = scale;
    }
    UpdateVertexBuffer();
}

void ImageView::FitPixelPerfect( const VkExtent2D& extent)
{
    m_fitToResize = false;
    m_extent = extent;
    m_imgOrigin = {
        ( (float)extent.width - m_bitmapExtent.width ) / 2,
        ( (float)extent.height - m_bitmapExtent.height ) / 2
    };
    m_imgScale = 1;
    UpdateVertexBuffer();
}

void ImageView::Pan( const Vector2<float>& delta )
{
    m_fitToResize = false;
    m_imgOrigin += delta;
    ClampImagePosition();
    UpdateVertexBuffer();
}

void ImageView::Zoom( const Vector2<float>& focus, float factor )
{
    m_fitToResize = false;
    const auto oldScale = m_imgScale;
    m_imgScale = std::clamp( m_imgScale * factor, 0.01f, 100.f );
    m_imgOrigin.x = focus.x + ( m_imgOrigin.x - focus.x ) * m_imgScale / oldScale;
    m_imgOrigin.y = focus.y + ( m_imgOrigin.y - focus.y ) * m_imgScale / oldScale;
    ClampImagePosition();
    UpdateVertexBuffer();
}

void ImageView::ClampImagePosition()
{
    m_imgOrigin.x = std::clamp( m_imgOrigin.x, m_extent.width / 2.f - m_bitmapExtent.width * m_imgScale, m_extent.width / 2.f );
    m_imgOrigin.y = std::clamp( m_imgOrigin.y, m_extent.height / 2.f - m_bitmapExtent.height * m_imgScale, m_extent.height / 2.f );
}

bool ImageView::HasBitmap()
{
    std::lock_guard lock( m_lock );
    return m_texture != nullptr;
}

void ImageView::CreatePipeline( VkFormat format )
{
    static constexpr VkVertexInputBindingDescription vertexBindingDescription = {
        .binding = 0,
        .stride = sizeof( Vertex ),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };
    static constexpr std::array vertexAttributeDescription = {
        VkVertexInputAttributeDescription { 0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof( Vertex, x ) },
        VkVertexInputAttributeDescription { 1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof( Vertex, u ) }
    };
    constexpr VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &vertexBindingDescription,
        .vertexAttributeDescriptionCount = vertexAttributeDescription.size(),
        .pVertexAttributeDescriptions = vertexAttributeDescription.data()
    };
    constexpr VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
    };
    constexpr VkPipelineViewportStateCreateInfo viewportState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1
    };
    constexpr VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .lineWidth = 1.0f
    };
    constexpr VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
    };
    static constexpr VkPipelineColorBlendAttachmentState colorBlendAttachment = {
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
    };
    constexpr VkPipelineColorBlendStateCreateInfo colorBlending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment
    };
    const std::array colorAttachmentFormats = {
        format
    };
    const VkPipelineRenderingCreateInfo rendering = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = colorAttachmentFormats.size(),
        .pColorAttachmentFormats = colorAttachmentFormats.data()
    };
    static constexpr std::array dynamicStateList = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    constexpr VkPipelineDynamicStateCreateInfo dynamicState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = dynamicStateList.size(),
        .pDynamicStates = dynamicStateList.data()
    };
    const bool pq = format == VK_FORMAT_A2B10G10R10_UNORM_PACK32 || format == VK_FORMAT_A2R10G10B10_UNORM_PACK32;
    VkGraphicsPipelineCreateInfo pipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &rendering,
        .stageCount = pq ? m_shaderMin[1]->GetStageCount() : m_shaderMin[0]->GetStageCount(),
        .pStages = pq ? m_shaderMin[1]->GetStages() : m_shaderMin[0]->GetStages(),
        .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pColorBlendState = &colorBlending,
        .pDynamicState = &dynamicState,
        .layout = *m_pipelineLayout,
    };
    m_pipelineMin = std::make_shared<VlkPipeline>( *m_device, pipelineInfo );

    pipelineInfo.stageCount = pq ? m_shaderExact[1]->GetStageCount() : m_shaderExact[0]->GetStageCount();
    pipelineInfo.pStages = pq ? m_shaderExact[1]->GetStages() : m_shaderExact[0]->GetStages();
    m_pipelineExact = std::make_shared<VlkPipeline>( *m_device, pipelineInfo );
}

void ImageView::Cleanup()
{
    if( m_texture )
    {
        m_garbage.Recycle( {
            std::move( m_texture ),
            std::move( m_vertexBuffer ),
        } );
    }
}

std::array<ImageView::Vertex, 4> ImageView::SetupVertexBuffer() const
{
    float x0 = std::floor( m_imgOrigin.x );
    float y0 = std::floor( m_imgOrigin.y );
    float x1 = std::round( x0 + m_bitmapExtent.width * m_imgScale );
    float y1 = std::round( y0 + m_bitmapExtent.height * m_imgScale );

    return { {
        { x0, y0, 0, 0 },
        { x1, y0, 1, 0 },
        { x1, y1, 1, 1 },
        { x0, y1, 0, 1 }
    } };
}

void ImageView::UpdateVertexBuffer()
{
    constexpr VkBufferCreateInfo vinfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof( Vertex ) * 4,
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    auto vb = std::make_shared<VlkBuffer>( *m_device, vinfo, VlkBuffer::PreferDevice | VlkBuffer::WillWrite );

    const auto vdata = SetupVertexBuffer();
    memcpy( vb->Ptr(), vdata.data(), sizeof( Vertex ) * 4 );
    vb->Flush();

    if( m_vertexBuffer ) m_garbage.Recycle( std::move( m_vertexBuffer ) );
    std::swap( m_vertexBuffer, vb );
}
