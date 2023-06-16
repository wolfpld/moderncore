#include <string.h>

#include "SoftwareCursor.hpp"
#include "Texture.hpp"
#include "../cursor/CursorBase.hpp"
#include "../cursor/CursorTheme.hpp"
#include "../util/Bitmap.hpp"
#include "../util/FileBuffer.hpp"
#include "../vulkan/VlkBuffer.hpp"
#include "../vulkan/VlkDevice.hpp"
#include "../vulkan/VlkImage.hpp"
#include "../vulkan/VlkImageView.hpp"
#include "../vulkan/VlkPipeline.hpp"
#include "../vulkan/VlkPipelineLayout.hpp"
#include "../vulkan/VlkSampler.hpp"
#include "../vulkan/VlkShader.hpp"
#include "../vulkan/VlkShaderModule.hpp"

SoftwareCursor::SoftwareCursor( VlkDevice& device, VkRenderPass renderPass, uint32_t screenWidth, uint32_t screenHeight )
{
    FileBuffer vert( "SoftwareCursor.vert.spv" );
    FileBuffer frag( "SoftwareCursor.frag.spv" );

    VlkShader::Stage stages[] = {
        { std::make_shared<VlkShaderModule>( device, vert.Data(), vert.Size() ), VK_SHADER_STAGE_VERTEX_BIT },
        { std::make_shared<VlkShaderModule>( device, frag.Data(), frag.Size() ), VK_SHADER_STAGE_FRAGMENT_BIT }
    };

    m_shader = std::make_unique<VlkShader>( stages, 2 );

    VkVertexInputBindingDescription bindingDescription = {};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof( Vertex );
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attributeDescriptions[] = {
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

    VkPipelineColorBlendStateCreateInfo colorBlending = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
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
    bufferInfo.size = sizeof( Vertex ) * 4;
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    m_vertexBuffer = std::make_unique<VlkBuffer>( device, bufferInfo, VlkBuffer::WillWrite );

    bufferInfo.size = sizeof( uint16_t ) * 6;
    bufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

    m_indexBuffer = std::make_unique<VlkBuffer>( device, bufferInfo, VlkBuffer::WillWrite );

    CursorTheme theme;
    auto& cursor = theme.Cursor();
    const auto size = cursor.FitSize( 24 );
    const auto& bitmaps = *cursor.Get( size, CursorType::Default );
    auto bmp = bitmaps[0];

    VkSamplerCreateInfo samplerInfo = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    m_sampler = std::make_unique<VlkSampler>( device, samplerInfo );

    m_image = std::make_unique<Texture>( device, *bmp.bitmap );

    auto stagingBuffer = std::make_unique<VlkBuffer>( device, bufferInfo, VlkBuffer::WillWrite | VlkBuffer::PreferHost );
    memcpy( stagingBuffer->Ptr(), bmp.bitmap->Data(), bmp.bitmap->Width() * bmp.bitmap->Height() * 4 );
    stagingBuffer->Flush( 0, bmp.bitmap->Width() * bmp.bitmap->Height() * 4 );

    Vertex vertices[] = {
        { { 0, 0 }, { 0, 0 } },
        { { 0, bmp.bitmap->Height() }, { 0, 1 } },
        { { bmp.bitmap->Width(), 0 }, { 1, 0 } },
        { { bmp.bitmap->Width(), bmp.bitmap->Height() }, { 1, 1 } }
    };

    memcpy( m_vertexBuffer->Ptr(), vertices, sizeof( vertices ) );
    m_vertexBuffer->Flush( 0, sizeof( vertices ) );

    uint16_t indices[] = { 0, 1, 2, 2, 1, 3 };
    memcpy( m_indexBuffer->Ptr(), indices, sizeof( indices ) );
    m_indexBuffer->Flush( 0, sizeof( indices ) );
}

SoftwareCursor::~SoftwareCursor()
{
}

void SoftwareCursor::SetPosition( float x, float y )
{
    m_x = x;
    m_y = y;
}

void SoftwareCursor::Render( VkCommandBuffer cmdBuf )
{
    Vertex vertices[] = {
        { { m_x, m_y }, { 0, 0 } },
        { { m_x, m_y + 24 }, { 0, 1 } },
        { { m_x + 24, m_y }, { 1, 0 } },
        { { m_x + 24, m_y + 24 }, { 1, 1 } }
    };

    memcpy( m_vertexBuffer->Ptr(), vertices, sizeof( vertices ) );
    m_vertexBuffer->Flush( 0, sizeof( vertices ) );

    std::array<VkBuffer, 1> vtx = { *m_vertexBuffer };
    std::array<VkDeviceSize, 1> offsets = { 0 };

    vkCmdBindPipeline( cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline );
    vkCmdBindVertexBuffers( cmdBuf, 0, 1, vtx.data(), offsets.data() );
    vkCmdBindIndexBuffer( cmdBuf, *m_indexBuffer, 0, VK_INDEX_TYPE_UINT16 );
    vkCmdDrawIndexed( cmdBuf, 6, 1, 0, 0, 0 );
}
