#include <array>
#include <math.h>
#include <string.h>

#include "Selection.hpp"
#include "util/EmbedData.hpp"
#include "vulkan/VlkBuffer.hpp"
#include "vulkan/VlkCommandBuffer.hpp"
#include "vulkan/VlkDevice.hpp"
#include "vulkan/VlkPipeline.hpp"
#include "vulkan/VlkPipelineLayout.hpp"
#include "vulkan/VlkShader.hpp"
#include "vulkan/VlkShaderModule.hpp"
#include "vulkan/ext/GarbageChute.hpp"
#include "vulkan/ext/Tracy.hpp"

#include "shader/SelectionFrag.hpp"
#include "shader/SelectionVert.hpp"


struct Vertex
{
    float x, y;
};

struct PushConstant
{
    float screenSize[2];
    float div;
    float offset;
};

Selection::Selection( GarbageChute& garbage, std::shared_ptr<VlkDevice> device, VkFormat format, float scale )
    : m_garbage( garbage )
    , m_device( std::move( device ) )
{
    SetScale( scale );

    Unembed( SelectionVert );
    Unembed( SelectionFrag );

    const std::array stages = {
        VlkShader::Stage { std::make_shared<VlkShaderModule>( *m_device, *SelectionVert ), VK_SHADER_STAGE_VERTEX_BIT },
        VlkShader::Stage { std::make_shared<VlkShaderModule>( *m_device, *SelectionFrag ), VK_SHADER_STAGE_FRAGMENT_BIT }
    };
    m_shader = std::make_shared<VlkShader>( stages );


    const std::array stagesPq = {
        stages[0],
        VlkShader::Stage { std::make_shared<VlkShaderModule>( *m_device, *SelectionFrag ), VK_SHADER_STAGE_FRAGMENT_BIT }
    };
    m_shaderPq = std::make_shared<VlkShader>( stagesPq );


    static constexpr std::array pushConstantRange = {
        VkPushConstantRange {
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .offset = offsetof( PushConstant, screenSize ),
            .size = sizeof( float ) * 2
        },
        VkPushConstantRange {
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset = offsetof( PushConstant, div ),
            .size = sizeof( float ) * 2
        }
    };
    constexpr VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pushConstantRangeCount = pushConstantRange.size(),
        .pPushConstantRanges = pushConstantRange.data()
    };
    m_pipelineLayout = std::make_shared<VlkPipelineLayout>( *m_device, pipelineLayoutInfo );
    CreatePipeline( format );


    constexpr Vertex vdata[] = {
        { -0.5f, -0.5f },
        {  0.5f, -0.5f },
        {  0.5f,  0.5f },
        { -0.5f,  0.5f }
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

    constexpr uint16_t idata[] = { 0, 1, 2, 3, 0 };
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

Selection::~Selection()
{
    m_garbage.Recycle( {
        std::move( m_pipeline ),
        std::move( m_pipelineLayout ),
        std::move( m_shader ),
        std::move( m_shaderPq ),
        std::move( m_vertexBuffer ),
        std::move( m_indexBuffer )
    } );
}

void Selection::Update( float delta )
{
    m_offset += delta * 20;
    m_offset = fmod( m_offset, 2 * M_PI / m_div );
}

void Selection::Render( VlkCommandBuffer& cmdbuf, const VkExtent2D& extent )
{
    VkViewport viewport = {
        .width = float( extent.width ),
        .height = float( extent.height )
    };
    VkRect2D scissor = {
        .extent = extent
    };

    const PushConstant pushConstant = {
        float( extent.width ),
        float( extent.height ),
        m_div,
        m_offset
    };

    std::array<VkBuffer, 1> vertexBuffers = { *m_vertexBuffer };
    constexpr std::array<VkDeviceSize, 1> offsets = { 0 };

    ZoneVk( *m_device, cmdbuf, "Selection", true );
    vkCmdBindPipeline( cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline );
    vkCmdPushConstants( cmdbuf, *m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, offsetof( PushConstant, screenSize ), sizeof( float ) * 2, &pushConstant );
    vkCmdPushConstants( cmdbuf, *m_pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, offsetof( PushConstant, div ), sizeof( float ) * 2, &pushConstant.div );
    vkCmdSetViewport( cmdbuf, 0, 1, &viewport );
    vkCmdSetScissor( cmdbuf, 0, 1, &scissor );
    vkCmdBindVertexBuffers( cmdbuf, 0, 1, vertexBuffers.data(), offsets.data() );
    vkCmdBindIndexBuffer( cmdbuf, *m_indexBuffer, 0, VK_INDEX_TYPE_UINT16 );
    vkCmdDrawIndexed( cmdbuf, 5, 1, 0, 0, 0 );
}

void Selection::SetScale( float scale )
{
    // Use 180° as a base to make the divider specify width
    // of a single segment, not the full white + black cycle.
    m_div = M_PI / 4.f / scale;
}

void Selection::FormatChange( VkFormat format )
{
    m_garbage.Recycle( std::move( m_pipeline ) );
    CreatePipeline( format );
}

void Selection::CreatePipeline( VkFormat format )
{
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
        .topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP
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
    const bool pq = format == VK_FORMAT_A2B10G10R10_UNORM_PACK32 || format == VK_FORMAT_A2R10G10B10_UNORM_PACK32;
    const VkGraphicsPipelineCreateInfo pipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &rendering,
        .stageCount = pq ? m_shaderPq->GetStageCount() : m_shader->GetStageCount(),
        .pStages = pq ? m_shaderPq->GetStages() : m_shader->GetStages(),
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
