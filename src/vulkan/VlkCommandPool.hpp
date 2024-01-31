#pragma once

#include <vulkan/vulkan.h>

#include "VlkQueueType.hpp"
#include "util/NoCopy.hpp"

class VlkCommandPool
{
public:
    VlkCommandPool( VkDevice device, int queueIndex, QueueType queueType );
    ~VlkCommandPool();

    NoCopy( VlkCommandPool );

    operator VkCommandPool() const { return m_pool; }
    operator VkDevice() const { return m_device; }
    operator QueueType() const { return m_queueType; }

private:
    VkCommandPool m_pool;
    VkDevice m_device;

    const QueueType m_queueType;
};
