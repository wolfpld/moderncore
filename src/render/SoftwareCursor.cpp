#include <array>
#include <string.h>

#include "SoftwareCursor.hpp"
#include "Texture.hpp"
#include "cursor/CursorBase.hpp"
#include "cursor/CursorLogic.hpp"
#include "util/Bitmap.hpp"
#include "util/FileBuffer.hpp"
#include "util/Panic.hpp"
#include "vulkan/VlkBuffer.hpp"
#include "vulkan/VlkDescriptorSetLayout.hpp"
#include "vulkan/VlkDevice.hpp"
#include "vulkan/VlkPipeline.hpp"
#include "vulkan/VlkPipelineLayout.hpp"
#include "vulkan/VlkProxy.hpp"
#include "vulkan/VlkSampler.hpp"
#include "vulkan/VlkShader.hpp"
#include "vulkan/VlkShaderModule.hpp"

SoftwareCursor::SoftwareCursor( VlkDevice& device, VkRenderPass renderPass, uint32_t screenWidth, uint32_t screenHeight )
    : m_x( 0 )
    , m_y( 0 )
    , m_w( 0 )
    , m_h( 0 )
    , m_currentBitmap( nullptr )
    , m_device( device )
{
    FileBuffer vert( "SoftwareCursor.vert.spv" );
    FileBuffer frag( "SoftwareCursor.frag.spv" );

    std::array stages = {
        VlkShader::Stage { std::make_shared<VlkShaderModule>( device, vert ), VK_SHADER_STAGE_VERTEX_BIT },
        VlkShader::Stage { std::make_shared<VlkShaderModule>( device, frag ), VK_SHADER_STAGE_FRAGMENT_BIT }
    };

    m_shader = std::make_unique<VlkShader>( stages );

    constexpr std::array bindings = {
        VkDescriptorSetLayoutBinding { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT },
        VkDescriptorSetLayoutBinding { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT }
    };

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    descriptorSetLayoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
    descriptorSetLayoutInfo.bindingCount = bindings.size();
    descriptorSetLayoutInfo.pBindings = bindings.data();

    m_descriptorSetLayout = std::make_unique<VlkDescriptorSetLayout>( device, descriptorSetLayoutInfo );

    VkVertexInputBindingDescription bindingDescription = {};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof( Vertex );
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    constexpr VkVertexInputAttributeDescription attributeDescriptions[] = {
        { 0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof( Vertex, pos ) },
        { 1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof( Vertex, uv ) }
    };

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = 2;
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport viewport = {};
    viewport.width = float( screenWidth );
    viewport.height = float( screenHeight );
    viewport.maxDepth = 1;

    VkRect2D scissor = {};
    scissor.extent.width = screenWidth;
    scissor.extent.height = screenHeight;

    VkPipelineViewportStateCreateInfo viewportState = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.f;

    VkPipelineMultisampleStateCreateInfo multisampling = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlending = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    VkPushConstantRange pushConstantRange = {};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof( Position );

    const std::array<VkDescriptorSetLayout, 1> layouts = { *m_descriptorSetLayout };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pipelineLayoutInfo.setLayoutCount = layouts.size();
    pipelineLayoutInfo.pSetLayouts = layouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    m_pipelineLayout = std::make_unique<VlkPipelineLayout>( device, pipelineLayoutInfo );

    VkGraphicsPipelineCreateInfo pipelineInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    pipelineInfo.stageCount = m_shader->GetStageCount();
    pipelineInfo.pStages = m_shader->GetStages();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = *m_pipelineLayout;
    pipelineInfo.renderPass = renderPass;

    m_pipeline = std::make_unique<VlkPipeline>( device, pipelineInfo );

    VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferInfo.size = sizeof( uint16_t ) * 6;
    bufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    m_indexBuffer = std::make_unique<VlkBuffer>( device, bufferInfo, VlkBuffer::WillWrite );

    bufferInfo.size = sizeof( ScreenUniform );
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

    m_uniformBuffer = std::make_unique<VlkBuffer>( device, bufferInfo, VlkBuffer::WillWrite );

    VkSamplerCreateInfo samplerInfo = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    m_sampler = std::make_unique<VlkSampler>( device, samplerInfo );

    constexpr uint16_t indices[] = { 0, 1, 2, 2, 1, 3 };
    memcpy( m_indexBuffer->Ptr(), indices, sizeof( indices ) );
    m_indexBuffer->Flush();

    const ScreenUniform uniform = { { float( screenWidth ), float( screenHeight ) } };
    memcpy( m_uniformBuffer->Ptr(), &uniform, sizeof( uniform ) );
    m_uniformBuffer->Flush();

    m_imageInfo = {};
    m_imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    m_imageInfo.sampler = *m_sampler;

    m_uniformInfo = {};
    m_uniformInfo.buffer = *m_uniformBuffer;
    m_uniformInfo.offset = 0;
    m_uniformInfo.range = sizeof( ScreenUniform );

    m_descriptorWrite[0] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    m_descriptorWrite[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    m_descriptorWrite[0].descriptorCount = 1;
    m_descriptorWrite[0].pBufferInfo = &m_uniformInfo;

    m_descriptorWrite[1] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    m_descriptorWrite[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    m_descriptorWrite[1].descriptorCount = 1;
    m_descriptorWrite[1].pImageInfo = &m_imageInfo;
    m_descriptorWrite[1].dstBinding = 1;
}

SoftwareCursor::~SoftwareCursor()
{
}

void SoftwareCursor::SetPosition( float x, float y )
{
    m_x = x;
    m_y = y;
}

void SoftwareCursor::Render( VkCommandBuffer cmdBuf, CursorLogic& cursorLogic )
{
    // NeedUpdate() needs to be called first, as it pushes the logic forward.
    if( cursorLogic.NeedUpdate() || m_w == 0 )
    {
        auto& cursorData = cursorLogic.GetCurrentCursorFrame();

        const auto w = cursorData.bitmap->Width();
        const auto h = cursorData.bitmap->Height();
        if( w != m_w || h != m_h ) UpdateVertexBuffer( w, h );
        if( cursorData.bitmap.get() != m_currentBitmap ) UpdateImage( cursorData );
    }

    const std::array<VkBuffer, 1> vtx = { *m_vertexBuffer };
    const std::array<VkDeviceSize, 1> offsets = { 0 };
    const Position position = { { m_x - m_xhot, m_y - m_yhot } };

    vkCmdBindPipeline( cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline );
    vkCmdBindVertexBuffers( cmdBuf, 0, 1, vtx.data(), offsets.data() );
    vkCmdBindIndexBuffer( cmdBuf, *m_indexBuffer, 0, VK_INDEX_TYPE_UINT16 );
    CmdPushDescriptorSetKHR( cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineLayout, 0, 2, m_descriptorWrite );
    vkCmdPushConstants( cmdBuf, *m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof( Position ), &position );
    vkCmdDrawIndexed( cmdBuf, 6, 1, 0, 0, 0 );
}

void SoftwareCursor::UpdateVertexBuffer( uint32_t width, uint32_t height )
{
    CheckPanic( m_w != width || m_h != height, "Cursor size is unchanged" );
    m_w = width;
    m_h = height;

    VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferInfo.size = sizeof( Vertex ) * 4;
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    m_vertexBuffer = std::make_unique<VlkBuffer>( m_device, bufferInfo, VlkBuffer::WillWrite );

    float w = width;
    float h = height;

    const Vertex vertices[] = {
        { { 0, 0 }, { 0, 0 } },
        { { 0, h }, { 0, 1 } },
        { { w, 0 }, { 1, 0 } },
        { { w, h }, { 1, 1 } }
    };

    memcpy( m_vertexBuffer->Ptr(), vertices, sizeof( vertices ) );
    m_vertexBuffer->Flush();
}

void SoftwareCursor::UpdateImage( const CursorBitmap& cursorData )
{
    const auto cursorPtr = cursorData.bitmap.get();
    CheckPanic( cursorPtr != m_currentBitmap, "Cursor bitmap is unchanged" );
    m_currentBitmap = cursorPtr;

    m_image = std::make_unique<Texture>( m_device, *cursorPtr, VK_FORMAT_B8G8R8A8_SRGB );
    m_imageInfo.imageView = *m_image;

    m_xhot = cursorData.xhot;
    m_yhot = cursorData.yhot;
}
