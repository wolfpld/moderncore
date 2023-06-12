#include "VlkCommandPool.hpp"
#include "VlkError.hpp"

VlkCommandPool::VlkCommandPool( VkDevice device, int queueIndex )
    : m_device( device )
{
    VkCommandPoolCreateInfo info = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    info.queueFamilyIndex = queueIndex;

    VkVerify( vkCreateCommandPool( device, &info, nullptr, &m_pool ) );
}

VlkCommandPool::~VlkCommandPool()
{
    vkDestroyCommandPool( m_device, m_pool, nullptr );
}
