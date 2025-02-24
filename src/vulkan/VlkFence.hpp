#pragma once

#include <stdint.h>
#include <vulkan/vulkan.h>

#include "util/NoCopy.hpp"

class VlkFence
{
public:
    explicit VlkFence( VkDevice device, VkFenceCreateFlags flags = 0 );
    ~VlkFence();

    NoCopy( VlkFence );

    VkResult Wait( uint64_t timeout = UINT64_MAX );
    void Reset();

    operator VkFence&() { return m_fence; }

private:
    VkFence m_fence;
    VkDevice m_device;
};
