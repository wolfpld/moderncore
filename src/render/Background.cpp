#include <array>
#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "Background.hpp"
#include "Texture.hpp"
#include "../image/ImageLoader.hpp"
#include "../server/Connector.hpp"
#include "../server/GpuState.hpp"
#include "../util/Bitmap.hpp"
#include "../util/Config.hpp"
#include "../vulkan/VlkBuffer.hpp"
#include "../vulkan/VlkDescriptorSetLayout.hpp"
#include "../vulkan/VlkPipeline.hpp"
#include "../vulkan/VlkPipelineLayout.hpp"
#include "../vulkan/VlkProxy.hpp"
#include "../vulkan/VlkSampler.hpp"
#include "../vulkan/VlkShader.hpp"
#include "../vulkan/VlkShaderModule.hpp"

Background::Background( const GpuState& gpuState )
    : m_color {{{ 0.0f, 0.0f, 0.0f, 1.0f }}}
    , m_vert( "Background.vert.spv" )
    , m_frag( "Background.frag.spv" )
{
    Config cfg( "background.ini" );
    const auto cstr = cfg.Get( "Background", "Color", "404040" );
    if( strlen( cstr ) == 6 )
    {
        const auto color = strtol( cstr, nullptr, 16 );
        if( color != LONG_MAX && color != LONG_MIN )
        {
            assert( ( color & ~0xFFFFFF ) == 0 );

            m_color.color.float32[0] = ( color >> 16 ) / 255.0f;
            m_color.color.float32[1] = ( ( color >> 8 ) & 0xFF ) / 255.0f;
            m_color.color.float32[2] = ( color & 0xFF ) / 255.0f;
        }
    }

    const auto imgFn = cfg.Get( "Background", "Image", (const char*)nullptr );
    if( imgFn )
    {
        auto img = LoadImage( imgFn );
        if( img )
        {
            m_bitmap.reset( img );
        }
    }

    for( auto& c : gpuState.Connectors().List() )
    {
        AddConnector( *c.second );
    }
}

Background::~Background()
{
}

void Background::AddConnector( Connector& connector )
{
    if( !m_bitmap ) return;

    auto& device = connector.Device();
    const auto width = connector.Width();
    const auto height = connector.Height();

    auto data = std::make_unique<DrawData>();

    std::array stages = {
        VlkShader::Stage { std::make_shared<VlkShaderModule>( device, m_vert ), VK_SHADER_STAGE_VERTEX_BIT },
        VlkShader::Stage { std::make_shared<VlkShaderModule>( device, m_frag ), VK_SHADER_STAGE_FRAGMENT_BIT }
    };

    data->shader = std::make_unique<VlkShader>( stages );

    constexpr std::array bindings = {
        VkDescriptorSetLayoutBinding { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT }
    };

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    descriptorSetLayoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
    descriptorSetLayoutInfo.bindingCount = bindings.size();
    descriptorSetLayoutInfo.pBindings = bindings.data();

    data->descriptorSetLayout = std::make_unique<VlkDescriptorSetLayout>( device, descriptorSetLayoutInfo );

    VkVertexInputBindingDescription vertexBindingDescription = {};
    vertexBindingDescription.binding = 0;
    vertexBindingDescription.stride = sizeof( Vertex );
    vertexBindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    constexpr VkVertexInputAttributeDescription attributeDescriptions[] = {
        { 0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof( Vertex, pos ) },
        { 1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof( Vertex, uv ) }
    };

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &vertexBindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = 2;
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport viewport = {};
    viewport.width = float( width );
    viewport.height = float( height );
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = {};
    scissor.extent.width = width;
    scissor.extent.height = height;

    VkPipelineViewportStateCreateInfo viewportState = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampling = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlending = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    const std::array<VkDescriptorSetLayout, 1> layouts = { *data->descriptorSetLayout };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pipelineLayoutInfo.setLayoutCount = layouts.size();
    pipelineLayoutInfo.pSetLayouts = layouts.data();

    data->pipelineLayout = std::make_unique<VlkPipelineLayout>( device, pipelineLayoutInfo );

    VkGraphicsPipelineCreateInfo pipelineInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    pipelineInfo.stageCount = data->shader->GetStageCount();
    pipelineInfo.pStages = data->shader->GetStages();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = *data->pipelineLayout;
    pipelineInfo.renderPass = connector.RenderPass();

    data->pipeline = std::make_unique<VlkPipeline>( device, pipelineInfo );

    VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferInfo.size = sizeof( Vertex ) * 4;
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    data->vertexBuffer = std::make_unique<VlkBuffer>( device, bufferInfo, VlkBuffer::WillWrite );

    bufferInfo.size = sizeof( uint16_t ) * 6;
    bufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

    data->indexBuffer = std::make_unique<VlkBuffer>( device, bufferInfo, VlkBuffer::WillWrite );

    constexpr uint16_t indices[] = { 0, 1, 2, 2, 3, 0 };
    memcpy( data->indexBuffer->Ptr(), indices, sizeof( indices ) );
    data->indexBuffer->Flush();

    VkSamplerCreateInfo samplerInfo = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    data->sampler = std::make_unique<VlkSampler>( device, samplerInfo );

    data->imageInfo = {};
    data->imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    data->imageInfo.sampler = *data->sampler;

    data->descriptorWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    data->descriptorWrite.descriptorCount = 1;
    data->descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    data->descriptorWrite.pImageInfo = &data->imageInfo;

    data->texture = std::make_unique<Texture>( device, *m_bitmap, VK_FORMAT_R8G8B8A8_SRGB );
    data->imageInfo.imageView = *data->texture;
    UpdateVertexBuffer( *data->vertexBuffer, m_bitmap->Width(), m_bitmap->Height(), width, height );

    m_drawData.emplace( &connector, std::move( data ) );
}

void Background::Render( Connector& connector, VkCommandBuffer cmdBuf )
{
    if( !m_bitmap ) return;

    auto it = m_drawData.find( &connector );
    assert( it != m_drawData.end() );
    auto& data = it->second;

    const std::array<VkBuffer, 1> vtx = { *data->vertexBuffer };
    const std::array<VkDeviceSize, 1> offsets = { 0 };

    vkCmdBindPipeline( cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, *data->pipeline );
    vkCmdBindVertexBuffers( cmdBuf, 0, 1, vtx.data(), offsets.data() );
    vkCmdBindIndexBuffer( cmdBuf, *data->indexBuffer, 0, VK_INDEX_TYPE_UINT16 );
    CmdPushDescriptorSetKHR( cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, *data->pipelineLayout, 0, 1, &data->descriptorWrite );
    vkCmdDrawIndexed( cmdBuf, 6, 1, 0, 0, 0 );
}

void Background::UpdateVertexBuffer( VlkBuffer& buffer, uint32_t imageWidth, uint32_t imageHeight, uint32_t displayWidth, uint32_t displayHeight )
{
    const float widthRatio = float( displayWidth ) / imageWidth;
    const float heightRatio = float( displayHeight ) / imageHeight;

    std::array<Vertex, 4> vertices;

    if( widthRatio > heightRatio )
    {
        const auto height = imageHeight * widthRatio;
        const auto offset = ( height - displayHeight ) / 2.0f;

        vertices = {
            Vertex { { -1.0f, -1.0f }, { 0.0f, offset / height } },
            Vertex { {  1.0f, -1.0f }, { 1.0f, offset / height } },
            Vertex { {  1.0f,  1.0f }, { 1.0f, ( height - offset ) / height } },
            Vertex { { -1.0f,  1.0f }, { 0.0f, ( height - offset ) / height } }
        };
    }
    else
    {
        const auto width = imageWidth * heightRatio;
        const auto offset = ( width - displayWidth ) / 2.0f;

        vertices = {
            Vertex { { -1.0f, -1.0f }, { offset / width,             0.0f } },
            Vertex { {  1.0f, -1.0f }, { ( width - offset ) / width, 0.0f } },
            Vertex { {  1.0f,  1.0f }, { ( width - offset ) / width, 1.0f } },
            Vertex { { -1.0f,  1.0f }, { offset / width,             1.0f } }
        };
    }

    memcpy( buffer.Ptr(), vertices.data(), sizeof( vertices ) );
    buffer.Flush();
}
