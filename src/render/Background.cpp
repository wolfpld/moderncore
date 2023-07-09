#include <array>
#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "Background.hpp"
#include "Texture.hpp"
#include "../image/ImageLoader.hpp"
#include "../util/Bitmap.hpp"
#include "../util/Config.hpp"
#include "../util/FileBuffer.hpp"
#include "../vulkan/VlkBuffer.hpp"
#include "../vulkan/VlkDescriptorSetLayout.hpp"
#include "../vulkan/VlkPipeline.hpp"
#include "../vulkan/VlkPipelineLayout.hpp"
#include "../vulkan/VlkProxy.hpp"
#include "../vulkan/VlkSampler.hpp"
#include "../vulkan/VlkShader.hpp"
#include "../vulkan/VlkShaderModule.hpp"

Background::Background( VlkDevice& device, VkRenderPass renderPass, uint32_t screenWidth, uint32_t screenHeight )
    : m_color {{{ 0.0f, 0.0f, 0.0f, 1.0f }}}
    , m_screenWidth( screenWidth )
    , m_screenHeight( screenHeight )
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

    FileBuffer vert( "Background.vert.spv" );
    FileBuffer frag( "Background.frag.spv" );

    std::array stages = {
        VlkShader::Stage { std::make_shared<VlkShaderModule>( device, vert ), VK_SHADER_STAGE_VERTEX_BIT },
        VlkShader::Stage { std::make_shared<VlkShaderModule>( device, frag ), VK_SHADER_STAGE_FRAGMENT_BIT }
    };

    m_shader = std::make_unique<VlkShader>( stages );

    constexpr std::array bindings = {
        VkDescriptorSetLayoutBinding { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT }
    };

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    descriptorSetLayoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
    descriptorSetLayoutInfo.bindingCount = bindings.size();
    descriptorSetLayoutInfo.pBindings = bindings.data();

    m_descriptorSetLayout = std::make_unique<VlkDescriptorSetLayout>( device, descriptorSetLayoutInfo );

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
    viewport.width = float( screenWidth );
    viewport.height = float( screenHeight );
    viewport.maxDepth = 1.0f;

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
    rasterizer.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampling = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlending = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    const std::array<VkDescriptorSetLayout, 1> layouts = { *m_descriptorSetLayout };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pipelineLayoutInfo.setLayoutCount = layouts.size();
    pipelineLayoutInfo.pSetLayouts = layouts.data();

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

    constexpr uint16_t indices[] = { 0, 1, 2, 2, 3, 0 };
    memcpy( m_indexBuffer->Ptr(), indices, sizeof( indices ) );
    m_indexBuffer->Flush();

    VkSamplerCreateInfo samplerInfo = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    m_sampler = std::make_unique<VlkSampler>( device, samplerInfo );

    m_imageInfo = {};
    m_imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    m_imageInfo.sampler = *m_sampler;

    m_descriptorWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    m_descriptorWrite.descriptorCount = 1;
    m_descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    m_descriptorWrite.pImageInfo = &m_imageInfo;

    const auto imgFn = cfg.Get( "Background", "Image", (const char*)nullptr );
    if( imgFn )
    {
        auto img = LoadImage( imgFn );
        if( img )
        {
            m_bitmap.reset( img );
            m_texture = std::make_unique<Texture>( device, *m_bitmap, VK_FORMAT_R8G8B8A8_SRGB );
            m_imageInfo.imageView = *m_texture;
            UpdateVertexBuffer( m_bitmap->Width(), m_bitmap->Height() );
        }
    }
}

Background::~Background()
{
}

void Background::Render( VkCommandBuffer cmdBuf )
{
    if( !m_texture ) return;

    const std::array<VkBuffer, 1> vtx = { *m_vertexBuffer };
    const std::array<VkDeviceSize, 1> offsets = { 0 };

    vkCmdBindPipeline( cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline );
    vkCmdBindVertexBuffers( cmdBuf, 0, 1, vtx.data(), offsets.data() );
    vkCmdBindIndexBuffer( cmdBuf, *m_indexBuffer, 0, VK_INDEX_TYPE_UINT16 );
    CmdPushDescriptorSetKHR( cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineLayout, 0, 1, &m_descriptorWrite );
    vkCmdDrawIndexed( cmdBuf, 6, 1, 0, 0, 0 );
}

void Background::UpdateVertexBuffer( uint32_t imageWidth, uint32_t imageHeight )
{
    const float widthRatio = float( m_screenWidth ) / imageWidth;
    const float heightRatio = float( m_screenHeight ) / imageHeight;

    std::array<Vertex, 4> vertices;

    if( widthRatio > heightRatio )
    {
        const auto height = imageHeight * widthRatio;
        const auto offset = ( height - m_screenHeight ) / 2.0f;

        vertices = {
            Vertex { { -1.0f, -1.0f }, { 0.0f, offset / height } },
            Vertex { {  1.0f, -1.0f }, { 1.0f, offset / height } },
            Vertex { {  1.0f,  1.0f }, { 1.0f, ( height - offset ) / height } },
            Vertex { { -1.0f,  1.0f }, { 0.0f, ( height - offset ) / height } }
        };

        memcpy( m_vertexBuffer->Ptr(), vertices.data(), sizeof( vertices ) );
        m_vertexBuffer->Flush();
    }
    else
    {
        const auto width = imageWidth * heightRatio;
        const auto offset = ( width - m_screenWidth ) / 2.0f;

        vertices = {
            Vertex { { -1.0f, -1.0f }, { offset / width,             0.0f } },
            Vertex { {  1.0f, -1.0f }, { ( width - offset ) / width, 0.0f } },
            Vertex { {  1.0f,  1.0f }, { ( width - offset ) / width, 1.0f } },
            Vertex { { -1.0f,  1.0f }, { offset / width,             1.0f } }
        };
    }

    memcpy( m_vertexBuffer->Ptr(), vertices.data(), sizeof( vertices ) );
    m_vertexBuffer->Flush();
}
