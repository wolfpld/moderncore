#pragma once

#include <mutex>
#include <vulkan/vulkan.h>

#include "VlkQueueType.hpp"
#include "util/NoCopy.hpp"

class VlkCommandPool
{
public:
    VlkCommandPool( VkDevice device, uint32_t queueIndex, QueueType queueType );
    ~VlkCommandPool();

    NoCopy( VlkCommandPool );

    operator VkCommandPool() const { return m_pool; }
    operator VkDevice() const { return m_device; }
    operator QueueType() const { return m_queueType; }

    void lock() const { m_lock.lock(); }
    void unlock() const { m_lock.unlock(); }

private:
    VkCommandPool m_pool;
    VkDevice m_device;

    const QueueType m_queueType;

    mutable std::mutex m_lock;
};
