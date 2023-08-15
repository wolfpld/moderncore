#pragma once

#include <vulkan/vulkan.h>

#include "../util/NoCopy.hpp"

class VlkSampler
{
public:
    VlkSampler( VkDevice device, const VkSamplerCreateInfo& createInfo );
    ~VlkSampler();

    NoCopy( VlkSampler );

    operator VkSampler() const { return m_sampler; }

private:
    VkSampler m_sampler;
    VkDevice m_device;
};
