#include "VlkCommandBuffer.hpp"
#include "VlkCommandPool.hpp"
#include "VlkError.hpp"

VlkCommandBuffer::VlkCommandBuffer( const VlkCommandPool& pool, bool primary )
    : m_commandPool( pool )
    , m_device( pool )
{
    VkCommandBufferAllocateInfo info  = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    info.commandPool = pool;
    info.level = primary ? VK_COMMAND_BUFFER_LEVEL_PRIMARY : VK_COMMAND_BUFFER_LEVEL_SECONDARY;
    info.commandBufferCount = 1;

    std::lock_guard lock( pool );
    VkVerify( vkAllocateCommandBuffers( pool, &info, &m_commandBuffer ) );
}

VlkCommandBuffer::~VlkCommandBuffer()
{
    std::lock_guard lock( m_commandPool );
    vkFreeCommandBuffers( m_device, m_commandPool, 1, &m_commandBuffer );
}

void VlkCommandBuffer::Begin( VkCommandBufferUsageFlags flags )
{
    VkCommandBufferBeginInfo begin = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    begin.flags = flags;
    m_commandPool.lock();
    VkVerify( vkBeginCommandBuffer( m_commandBuffer, &begin ) );
}

void VlkCommandBuffer::End()
{
    VkVerify( vkEndCommandBuffer( m_commandBuffer ) );
    m_commandPool.unlock();
}

void VlkCommandBuffer::Reset( VkCommandBufferResetFlags flags )
{
    VkVerify( vkResetCommandBuffer( m_commandBuffer, flags ) );
}
