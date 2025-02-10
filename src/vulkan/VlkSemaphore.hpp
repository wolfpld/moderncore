#pragma once

#include <vulkan/vulkan.h>

#include "VlkBase.hpp"
#include "util/NoCopy.hpp"

class VlkSemaphore : public VlkBase
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
