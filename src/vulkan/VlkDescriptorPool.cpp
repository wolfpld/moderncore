#include "VlkError.hpp"
#include "VlkDescriptorPool.hpp"

VlkDescriptorPool::VlkDescriptorPool( VkDevice device, const VkDescriptorPoolCreateInfo& createInfo )
    : m_device( device )
{
    VkVerify( vkCreateDescriptorPool( m_device, &createInfo, nullptr, &m_descriptorPool ) );
}

VlkDescriptorPool::~VlkDescriptorPool()
{
    vkDestroyDescriptorPool( m_device, m_descriptorPool, nullptr );
}
