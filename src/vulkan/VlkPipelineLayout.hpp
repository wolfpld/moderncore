#pragma once

#include <vulkan/vulkan.h>

#include "util/NoCopy.hpp"

class VlkPipelineLayout
{
public:
    VlkPipelineLayout( VkDevice device, const VkPipelineLayoutCreateInfo& createInfo );
    ~VlkPipelineLayout();

    NoCopy( VlkPipelineLayout );

    operator VkPipelineLayout() const { return m_layout; }

private:
    VkPipelineLayout m_layout;
    VkDevice m_device;
};
