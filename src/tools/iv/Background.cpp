#include <array>
#include <string.h>

#include "Background.hpp"
#include "util/EmbedData.hpp"
#include "vulkan/VlkBuffer.hpp"
#include "vulkan/VlkCommandBuffer.hpp"
#include "vulkan/VlkDevice.hpp"
#include "vulkan/VlkPipeline.hpp"
#include "vulkan/VlkPipelineLayout.hpp"
#include "vulkan/VlkShader.hpp"
#include "vulkan/VlkShaderModule.hpp"
#include "vulkan/ext/GarbageChute.hpp"

#include "shader/BackgroundFrag.hpp"
#include "shader/BackgroundVert.hpp"


struct Vertex
{
    float x, y;
};

Background::Background( GarbageChute& garbage, std::shared_ptr<VlkDevice> device, VkFormat format, float scale )
    : m_garbage( garbage )
    , m_device( std::move( device ) )
{
    SetScale( scale );

    Unembed( BackgroundVert );
    Unembed( BackgroundFrag );

    std::array stages = {
        VlkShader::Stage { std::make_shared<VlkShaderModule>( *m_device, BackgroundVert ), VK_SHADER_STAGE_VERTEX_BIT },
        VlkShader::Stage { std::make_shared<VlkShaderModule>( *m_device, BackgroundFrag ), VK_SHADER_STAGE_FRAGMENT_BIT }
    };
    m_shader = std::make_shared<VlkShader>( stages );


    static constexpr VkPushConstantRange pushConstantRange = {
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = 0,
        .size = sizeof( float )
    };
    constexpr VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange
    };
    m_pipelineLayout = std::make_shared<VlkPipelineLayout>( *m_device, pipelineLayoutInfo );


    static constexpr VkVertexInputBindingDescription vertexBindingDescription = {
        .binding = 0,
        .stride = sizeof( Vertex ),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };
    static constexpr std::array vertexAttributeDescription = {
        VkVertexInputAttributeDescription { 0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof( Vertex, x ) }
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


    constexpr Vertex vdata[] = {
        { -1, -1 },
        { -1,  3 },
        {  3, -1 }
    };
    constexpr VkBufferCreateInfo vinfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof( vdata ),
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    m_vertexBuffer = std::make_shared<VlkBuffer>( *m_device, vinfo, VlkBuffer::PreferDevice | VlkBuffer::WillWrite );
    memcpy( m_vertexBuffer->Ptr(), vdata, sizeof( vdata ) );
    m_vertexBuffer->Flush();
}

Background::~Background()
{
    m_garbage.Recycle( {
        std::move( m_pipeline ),
        std::move( m_pipelineLayout ),
        std::move( m_shader ),
        std::move( m_vertexBuffer )
    } );
}

void Background::Render( VlkCommandBuffer& cmdbuf, const VkExtent2D& extent )
{
    VkViewport viewport = {
        .width = float( extent.width ),
        .height = float( extent.height )
    };
    VkRect2D scissor = {
        .extent = extent
    };

    std::array<VkBuffer, 1> vertexBuffers = { *m_vertexBuffer };
    constexpr std::array<VkDeviceSize, 1> offsets = { 0 };

    vkCmdBindPipeline( cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline );
    vkCmdPushConstants( cmdbuf, *m_pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( float ), &m_div );
    vkCmdSetViewport( cmdbuf, 0, 1, &viewport );
    vkCmdSetScissor( cmdbuf, 0, 1, &scissor );
    vkCmdBindVertexBuffers( cmdbuf, 0, 1, vertexBuffers.data(), offsets.data() );
    vkCmdDraw( cmdbuf, 3, 1, 0, 0 );
}
