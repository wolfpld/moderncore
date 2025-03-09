#include "VlkCommandBuffer.hpp"
#include "VlkCommandPool.hpp"
#include "VlkError.hpp"

VlkCommandBuffer::VlkCommandBuffer( const VlkCommandPool& pool, bool primary )
    : m_commandPool( pool )
    , m_device( pool )
{
    const VkCommandBufferAllocateInfo info  = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = pool,
        .level = primary ? VK_COMMAND_BUFFER_LEVEL_PRIMARY : VK_COMMAND_BUFFER_LEVEL_SECONDARY,
        .commandBufferCount = 1
    };

    std::lock_guard lock( pool );
    VkVerify( vkAllocateCommandBuffers( pool, &info, &m_commandBuffer ) );
}

VlkCommandBuffer::~VlkCommandBuffer()
{
    std::lock_guard lock( m_commandPool );
    vkFreeCommandBuffers( m_device, m_commandPool, 1, &m_commandBuffer );
}

void VlkCommandBuffer::Begin( VkCommandBufferUsageFlags flags, bool _lock )
{
    constexpr VkCommandBufferBeginInfo begin = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };

    if( _lock ) lock();
    VkVerify( vkBeginCommandBuffer( m_commandBuffer, &begin ) );
}

void VlkCommandBuffer::End( bool _unlock )
{
    VkVerify( vkEndCommandBuffer( m_commandBuffer ) );
    if( _unlock ) m_commandPool.unlock();
}

void VlkCommandBuffer::Reset( VkCommandBufferResetFlags flags )
{
    VkVerify( vkResetCommandBuffer( m_commandBuffer, flags ) );
}
