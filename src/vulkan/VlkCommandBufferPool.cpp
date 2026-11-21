#include "VlkCommandBufferPool.hpp"
#include "VlkCommandPool.hpp"
#include "VlkError.hpp"

VlkCommandBufferPool::VlkCommandBufferPool( const VlkCommandPool& pool, uint32_t initialCount )
    : m_pool( pool )
{
    std::lock_guard lock( m_pool );
    m_all.reserve( initialCount );
    for( uint32_t i = 0; i < initialCount; i++ )
    {
        VkCommandBuffer cmdBuf = Allocate();
        m_all.push_back( cmdBuf );
        m_free.push_back( cmdBuf );
    }
}

VlkCommandBufferPool::~VlkCommandBufferPool()
{
    std::lock_guard lock( m_pool );
    vkFreeCommandBuffers( m_pool, m_pool, (uint32_t)m_all.size(), m_all.data() );
}

[[nodiscard]] VkCommandBuffer VlkCommandBufferPool::Allocate()
{
    const VkCommandBufferAllocateInfo info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = m_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    VkCommandBuffer cmdBuf;
    VkVerify( vkAllocateCommandBuffers( m_pool, &info, &cmdBuf ) );
    return cmdBuf;
}

VkCommandBuffer VlkCommandBufferPool::Acquire()
{
    m_pool.lock();
    if( m_free.empty() )
    {
        VkCommandBuffer fresh = Allocate();
        m_all.push_back( fresh );
        m_free.push_back( fresh );
    }
    const VkCommandBuffer cmdBuf = m_free.back();
    m_free.pop_back();

    const VkCommandBufferBeginInfo begin = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    VkVerify( vkResetCommandBuffer( cmdBuf, 0 ) );
    VkVerify( vkBeginCommandBuffer( cmdBuf, &begin ) );
    return cmdBuf;
}

void VlkCommandBufferPool::End( VkCommandBuffer cmdBuf )
{
    VkVerify( vkEndCommandBuffer( cmdBuf ) );
    m_pool.unlock();
}

void VlkCommandBufferPool::Release( VkCommandBuffer cmdBuf )
{
    m_free.push_back( cmdBuf );
}
