#pragma once

#include <vulkan/vulkan.h>

#include "VlkBase.hpp"
#include "VlkCommandPool.hpp"
#include "util/NoCopy.hpp"

class VlkCommandBuffer : public VlkBase
{
public:
    explicit VlkCommandBuffer( const VlkCommandPool& pool, bool primary = true );
    ~VlkCommandBuffer();

    NoCopy( VlkCommandBuffer );

    void Begin( VkCommandBufferUsageFlags flags = 0, bool lock = true );
    void End( bool unlock = true );

    void Reset( VkCommandBufferResetFlags flags = 0 );

    operator VkCommandBuffer() const { return m_commandBuffer; }
    operator QueueType() const { return m_commandPool; }

    void lock() { m_commandPool.lock(); }
    void unlock() { m_commandPool.unlock(); }

private:
    VkCommandBuffer m_commandBuffer;
    const VlkCommandPool& m_commandPool;
    VkDevice m_device;
};
