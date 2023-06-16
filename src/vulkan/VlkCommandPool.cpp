#include "VlkCommandPool.hpp"
#include "VlkError.hpp"

VlkCommandPool::VlkCommandPool( VkDevice device, int queueIndex, QueueType queueType )
    : m_device( device )
    , m_queueType( queueType )
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
