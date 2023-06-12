#ifndef __VLKFENCE_HPP__
#define __VLKFENCE_HPP__

#include <stdint.h>
#include <vulkan/vulkan.h>

class VlkFence
{
public:
    explicit VlkFence( VkDevice device, VkFenceCreateFlags flags = 0 );
    ~VlkFence();

    VkResult Wait( uint64_t timeout = UINT64_MAX );
    void Reset();

    operator VkFence() const { return m_fence; }

private:
    VkFence m_fence;
    VkDevice m_device;
};

#endif
