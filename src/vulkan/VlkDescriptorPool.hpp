#ifndef __VLKDESCRIPTORPOOL_HPP__
#define __VLKDESCRIPTORPOOL_HPP__

#include <vulkan/vulkan.h>

class VlkDescriptorPool
{
public:
    VlkDescriptorPool( VkDevice device, const VkDescriptorPoolCreateInfo& createInfo );
    ~VlkDescriptorPool();

    operator VkDescriptorPool() const { return m_descriptorPool; }

private:
    VkDevice m_device;
    VkDescriptorPool m_descriptorPool;
};

#endif
