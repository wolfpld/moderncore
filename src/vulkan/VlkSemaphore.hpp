#ifndef __VLKSEMAPHORE_HPP__
#define __VLKSEMAPHORE_HPP__

#include <vulkan/vulkan.h>

class VlkSemaphore
{
public:
    explicit VlkSemaphore( VkDevice device );
    ~VlkSemaphore();

    operator VkSemaphore() const { return m_semaphore; }

private:
    VkSemaphore m_semaphore;
    VkDevice m_device;
};

#endif
