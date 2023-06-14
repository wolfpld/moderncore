#include "SoftwareCursor.hpp"
#include "../util/FileBuffer.hpp"
#include "../vulkan/VlkDevice.hpp"
#include "../vulkan/VlkPipeline.hpp"
#include "../vulkan/VlkPipelineLayout.hpp"
#include "../vulkan/VlkShader.hpp"
#include "../vulkan/VlkShaderModule.hpp"

SoftwareCursor::SoftwareCursor( const VlkDevice& device, VkRenderPass renderPass, uint32_t screenWidth, uint32_t screenHeight )
{
    FileBuffer vert( "SoftwareCursor.vert.spv" );
    FileBuffer frag( "SoftwareCursor.frag.spv" );

    VlkShader::Stage stages[] = {
        { std::make_shared<VlkShaderModule>( device, vert.Data(), vert.Size() ), VK_SHADER_STAGE_VERTEX_BIT },
        { std::make_shared<VlkShaderModule>( device, frag.Data(), frag.Size() ), VK_SHADER_STAGE_FRAGMENT_BIT }
    };

    m_shader = std::make_unique<VlkShader>( stages, 2 );

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

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
}

SoftwareCursor::~SoftwareCursor()
{
}

void SoftwareCursor::SetPosition( float x, float y )
{
    m_x = x;
    m_y = y;
}

void SoftwareCursor::Render()
{
}
