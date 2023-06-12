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

    VkVerify( vkAllocateCommandBuffers( pool, &info, &m_commandBuffer ) );
}

VlkCommandBuffer::~VlkCommandBuffer()
{
    vkFreeCommandBuffers( m_device, m_commandPool, 1, &m_commandBuffer );
}

void VlkCommandBuffer::Begin( VkCommandBufferUsageFlags flags )
{
    VkCommandBufferBeginInfo begin = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    begin.flags = flags;
    VkVerify( vkBeginCommandBuffer( m_commandBuffer, &begin ) );
}

void VlkCommandBuffer::End()
{
    VkVerify( vkEndCommandBuffer( m_commandBuffer ) );
}

void VlkCommandBuffer::Reset( VkCommandBufferResetFlags flags )
{
    VkVerify( vkResetCommandBuffer( m_commandBuffer, flags ) );
}
