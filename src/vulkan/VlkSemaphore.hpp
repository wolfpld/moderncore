#pragma once

#include <vulkan/vulkan.h>

#include "util/NoCopy.hpp"

class VlkSemaphore
{
public:
    explicit VlkSemaphore( VkDevice device );
    ~VlkSemaphore();

    NoCopy( VlkSemaphore );

    operator VkSemaphore() const { return m_semaphore; }

private:
    VkSemaphore m_semaphore;
    VkDevice m_device;
};
