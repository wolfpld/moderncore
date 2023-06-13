#include "VlkError.hpp"
#include "VlkPipeline.hpp"

VlkPipeline::VlkPipeline( VkDevice device, const VkGraphicsPipelineCreateInfo& createInfo )
    : m_device( device )
{
    VkVerify( vkCreateGraphicsPipelines( device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &m_pipeline ) );
}

VlkPipeline::~VlkPipeline()
{
    vkDestroyPipeline( m_device, m_pipeline, nullptr );
}
