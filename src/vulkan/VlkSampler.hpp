#ifndef __VLKSAMPLER_HPP__
#define __VLKSAMPLER_HPP__

#include <vulkan/vulkan.h>

class VlkSampler
{
public:
    VlkSampler( VkDevice device, const VkSamplerCreateInfo& createInfo );
    ~VlkSampler();

    operator VkSampler() const { return m_sampler; }

private:
    VkSampler m_sampler;
    VkDevice m_device;
};

#endif
