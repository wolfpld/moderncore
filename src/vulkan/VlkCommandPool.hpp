#ifndef __VLKCOMMANDPOOL_HPP__
#define __VLKCOMMANDPOOL_HPP__

#include <vulkan/vulkan.h>

class VlkCommandPool
{
public:
    VlkCommandPool( VkDevice device, int queueIndex );
    ~VlkCommandPool();

    operator VkCommandPool() const { return m_pool; }
    operator VkDevice() const { return m_device; }

private:
    VkCommandPool m_pool;
    VkDevice m_device;
};

#endif
