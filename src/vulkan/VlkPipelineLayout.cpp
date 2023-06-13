#include "VlkError.hpp"
#include "VlkPipelineLayout.hpp"

VlkPipelineLayout::VlkPipelineLayout( VkDevice device, const VkPipelineLayoutCreateInfo& createInfo )
    : m_device( device )
{
    VkVerify( vkCreatePipelineLayout( device, &createInfo, nullptr, &m_layout ) );
}

VlkPipelineLayout::~VlkPipelineLayout()
{
    vkDestroyPipelineLayout( m_device, m_layout, nullptr );
}
