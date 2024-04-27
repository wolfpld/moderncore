#pragma once

#include <vulkan/vulkan.h>

#include "VlkBase.hpp"
#include "util/NoCopy.hpp"

class VlkSampler : public VlkBase
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
