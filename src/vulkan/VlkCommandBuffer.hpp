#pragma once

#include <vulkan/vulkan.h>

#include "VlkCommandPool.hpp"
#include "../util/NoCopy.hpp"

class VlkCommandBuffer
{
public:
    explicit VlkCommandBuffer( const VlkCommandPool& pool, bool primary = true );
    ~VlkCommandBuffer();

    NoCopy( VlkCommandBuffer );

    void Begin( VkCommandBufferUsageFlags flags = 0 );
    void End();

    void Reset( VkCommandBufferResetFlags flags = 0 );

    operator VkCommandBuffer() const { return m_commandBuffer; }
    operator QueueType() const { return m_commandPool; }

private:
    VkCommandBuffer m_commandBuffer;
    const VlkCommandPool& m_commandPool;
    VkDevice m_device;
};
