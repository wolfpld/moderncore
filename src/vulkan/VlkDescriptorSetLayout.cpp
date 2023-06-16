#include "VlkDescriptorSetLayout.hpp"
#include "VlkError.hpp"

VlkDescriptorSetLayout::VlkDescriptorSetLayout( const VlkDevice& device, const VkDescriptorSetLayoutCreateInfo& createInfo )
    : m_device( device )
{
    VkVerify( vkCreateDescriptorSetLayout( m_device, &createInfo, nullptr, &m_descriptorSetLayout ) );
}

VlkDescriptorSetLayout::~VlkDescriptorSetLayout()
{
    vkDestroyDescriptorSetLayout( m_device, m_descriptorSetLayout, nullptr );
}
