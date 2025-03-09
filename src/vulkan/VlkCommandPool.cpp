#include "VlkCommandPool.hpp"
#include "VlkError.hpp"

VlkCommandPool::VlkCommandPool( VkDevice device, uint32_t queueIndex, QueueType queueType )
    : m_device( device )
    , m_queueType( queueType )
{
    const VkCommandPoolCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queueIndex
    };
    VkVerify( vkCreateCommandPool( device, &info, nullptr, &m_pool ) );
}

VlkCommandPool::~VlkCommandPool()
{
    vkDestroyCommandPool( m_device, m_pool, nullptr );
}
