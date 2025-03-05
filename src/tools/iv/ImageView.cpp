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
#include "shader/TexturingVert.hpp"

struct PushConstant
{
    float screenSize[2];
    float texDelta;
    float div;
};

ImageView::ImageView( GarbageChute& garbage, std::shared_ptr<VlkDevice> device, VkFormat format, const VkExtent2D& extent, float scale )
    : m_garbage( garbage )
    , m_device( device )
    , m_extent( extent )
{
    SetScale( scale );

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


    Unembed( TexturingVert );
    Unembed( SupersampleFrag );

    std::array stages = {
        VlkShader::Stage { std::make_shared<VlkShaderModule>( *m_device, *TexturingVert ), VK_SHADER_STAGE_VERTEX_BIT },
        VlkShader::Stage { std::make_shared<VlkShaderModule>( *m_device, *SupersampleFrag ), VK_SHADER_STAGE_FRAGMENT_BIT }
    };
    m_shader = std::make_shared<VlkShader>( stages );


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
            .offset = offsetof( PushConstant, texDelta ),
            .size = sizeof( float ) * 2
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


    uint16_t idata[] = { 0, 1, 2, 2, 3, 0 };
    VkBufferCreateInfo iinfo = {
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
        std::move( m_pipeline ),
        std::move( m_pipelineLayout ),
        std::move( m_setLayout ),
        std::move( m_shader ),
        std::move( m_vertexBuffer ),
        std::move( m_indexBuffer ),
        std::move( m_texture ),
        std::move( m_sampler )
    } );
}

void ImageView::Render( VlkCommandBuffer& cmdbuf, const VkExtent2D& extent )
{
    CheckPanic( m_texture, "No texture" );

    VkViewport viewport = {
        .width = float( extent.width ),
        .height = float( extent.height )
    };
    VkRect2D scissor = {
        .extent = extent
    };

    PushConstant pushConstant = {
        float( extent.width ),
        float( extent.height ),
        1.f / float( m_bitmapExtent.width ),
        m_div
    };

    std::array<VkBuffer, 1> vertexBuffers = { *m_vertexBuffer };
    constexpr std::array<VkDeviceSize, 1> offsets = { 0 };

    std::lock_guard lock( m_lock );

    ZoneVk( *m_device, cmdbuf, "ImageView", true );
    vkCmdBindPipeline( cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline );
    vkCmdPushConstants( cmdbuf, *m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, offsetof( PushConstant, screenSize ), sizeof( float ) * 2, &pushConstant );
    vkCmdPushConstants( cmdbuf, *m_pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, offsetof( PushConstant, texDelta ), sizeof( float ) * 2, &pushConstant.texDelta );
    vkCmdSetViewport( cmdbuf, 0, 1, &viewport );
    vkCmdSetScissor( cmdbuf, 0, 1, &scissor );
    vkCmdBindVertexBuffers( cmdbuf, 0, 1, vertexBuffers.data(), offsets.data() );
    vkCmdBindIndexBuffer( cmdbuf, *m_indexBuffer, 0, VK_INDEX_TYPE_UINT16 );
    CmdPushDescriptorSetKHR( cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineLayout, 0, 1, &m_descWrite );
    vkCmdDrawIndexed( cmdbuf, 6, 1, 0, 0, 0 );
}

void ImageView::Resize( const VkExtent2D& extent )
{
    constexpr VkBufferCreateInfo vinfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof( Vertex ) * 4,
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    auto vb = std::make_shared<VlkBuffer>( *m_device, vinfo, VlkBuffer::PreferDevice | VlkBuffer::WillWrite );

    std::lock_guard lock( m_lock );
    m_extent = extent;

    const auto vdata = SetupVertexBuffer();
    memcpy( vb->Ptr(), vdata.data(), sizeof( Vertex ) * 4 );
    vb->Flush();

    std::swap( m_vertexBuffer, vb );
    m_garbage.Recycle( std::move( vb ) );
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
    auto texture = std::make_shared<Texture>( *m_device, *bitmap, VK_FORMAT_R32G32B32A32_SFLOAT, true, texFences, &td );

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

    m_bitmapExtent = { width, height };
    const auto vdata = SetupVertexBuffer();
    memcpy( vb->Ptr(), vdata.data(), sizeof( Vertex ) * 4 );
    vb->Flush();

    Cleanup();

    std::swap( m_texture, texture );
    std::swap( m_vertexBuffer, vb );

    m_imageInfo.imageView = *m_texture;
}

void ImageView::FormatChange( VkFormat format )
{
    m_garbage.Recycle( std::move( m_pipeline ) );
    CreatePipeline( format );
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
    const VkGraphicsPipelineCreateInfo pipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &rendering,
        .stageCount = m_shader->GetStageCount(),
        .pStages = m_shader->GetStages(),
        .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pColorBlendState = &colorBlending,
        .pDynamicState = &dynamicState,
        .layout = *m_pipelineLayout,
    };
    m_pipeline = std::make_shared<VlkPipeline>( *m_device, pipelineInfo );
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

std::array<ImageView::Vertex, 4> ImageView::SetupVertexBuffer()
{
    float x0, x1, y0, y1;

    if( m_bitmapExtent.width <= m_extent.width &&
        m_bitmapExtent.height <= m_extent.height )
    {
        x0 = ( m_extent.width - m_bitmapExtent.width ) / 2;
        x1 = x0 + m_bitmapExtent.width;
        y0 = ( m_extent.height - m_bitmapExtent.height ) / 2;
        y1 = y0 + m_bitmapExtent.height;
    }
    else
    {
        const auto wr = float( m_extent.width ) / m_bitmapExtent.width;
        const auto hr = float( m_extent.height ) / m_bitmapExtent.height;
        const auto r = std::min( wr, hr );

        const auto w = m_bitmapExtent.width * r;
        const auto h = m_bitmapExtent.height * r;

        x0 = ( m_extent.width - w ) / 2;
        x1 = x0 + w;
        y0 = ( m_extent.height - h ) / 2;
        y1 = y0 + h;
    }

    return { {
        { x0, y0, 0, 0 },
        { x1, y0, 1, 0 },
        { x1, y1, 1, 1 },
        { x0, y1, 0, 1 }
    } };
}
