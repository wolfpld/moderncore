#include <array>
#include <string.h>

#include "BusyIndicator.hpp"
#include "BusyIndicatorFrag.hpp"
#include "BusyIndicatorVert.hpp"
#include "HourglassSvg.hpp"
#include "image/vector/SvgImage.hpp"
#include "render/Texture.hpp"
#include "util/Bitmap.hpp"
#include "util/DataBuffer.hpp"
#include "util/EmbedData.hpp"
#include "vulkan/VlkBuffer.hpp"
#include "vulkan/VlkCommandBuffer.hpp"
#include "vulkan/VlkDescriptorSetLayout.hpp"
#include "vulkan/VlkDevice.hpp"
#include "vulkan/VlkPipeline.hpp"
#include "vulkan/VlkPipelineLayout.hpp"
#include "vulkan/VlkProxy.hpp"
#include "vulkan/VlkSampler.hpp"
#include "vulkan/VlkShader.hpp"
#include "vulkan/VlkShaderModule.hpp"
#include "vulkan/ext/GarbageChute.hpp"

struct Vertex
{
    float x, y;
};

struct PushConstant
{
    float mul[2];
};

constexpr size_t HourglassSize = 128;

BusyIndicator::BusyIndicator( GarbageChute& garbage, std::shared_ptr<VlkDevice> device, VkFormat format, float scale )
    : m_garbage( garbage )
    , m_device( device )
{
    Unembed( HourglassSvg );

    auto img = std::make_shared<SvgImage>( std::make_shared<DataBuffer>( (const char*)HourglassSvg.data(), HourglassSvg.size() ) );
    m_texture = std::make_shared<Texture>( *m_device, *img->Rasterize( HourglassSize, HourglassSize ), VK_FORMAT_R8G8B8A8_UNORM );

    VkSamplerCreateInfo samplerInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
    };
    m_sampler = std::make_unique<VlkSampler>( *m_device, samplerInfo );

    m_imageInfo = {
        .sampler = *m_sampler,
        .imageView = *m_texture,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    m_descWrite = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &m_imageInfo
    };


    Unembed( BusyIndicatorVert );
    Unembed( BusyIndicatorFrag );

    std::array stages = {
        VlkShader::Stage { std::make_shared<VlkShaderModule>( *m_device, BusyIndicatorVert ), VK_SHADER_STAGE_VERTEX_BIT },
        VlkShader::Stage { std::make_shared<VlkShaderModule>( *m_device, BusyIndicatorFrag ), VK_SHADER_STAGE_FRAGMENT_BIT }
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
    static constexpr VkPushConstantRange pushConstantRange = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = sizeof( PushConstant )
    };
    const VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = sets.size(),
        .pSetLayouts = sets.data(),
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

    constexpr Vertex vdata[] = {
        { -1, -1 },
        { -1,  1 },
        {  1,  1 },
        {  1, -1 }
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

BusyIndicator::~BusyIndicator()
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

void BusyIndicator::Render( VlkCommandBuffer& cmdbuf, const VkExtent2D& extent )
{
    VkViewport viewport = {
        .width = float( extent.width ),
        .height = float( extent.height )
    };
    VkRect2D scissor = {
        .extent = extent
    };

    PushConstant pushConstant = {
        float( HourglassSize ) / extent.width,
        float( HourglassSize ) / extent.height
    };

    std::array<VkBuffer, 1> vertexBuffers = { *m_vertexBuffer };
    constexpr std::array<VkDeviceSize, 1> offsets = { 0 };

    vkCmdBindPipeline( cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline );
    vkCmdPushConstants( cmdbuf, *m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof( PushConstant ), &pushConstant );
    vkCmdSetViewport( cmdbuf, 0, 1, &viewport );
    vkCmdSetScissor( cmdbuf, 0, 1, &scissor );
    vkCmdBindVertexBuffers( cmdbuf, 0, 1, vertexBuffers.data(), offsets.data() );
    vkCmdBindIndexBuffer( cmdbuf, *m_indexBuffer, 0, VK_INDEX_TYPE_UINT16 );
    CmdPushDescriptorSetKHR( cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineLayout, 0, 1, &m_descWrite );
    vkCmdDrawIndexed( cmdbuf, 6, 1, 0, 0, 0 );
}
