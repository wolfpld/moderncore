#pragma once

#include <vulkan/vulkan.h>

#include "../util/NoCopy.hpp"

class VlkPipeline
{
public:
    VlkPipeline( VkDevice device, const VkGraphicsPipelineCreateInfo& createInfo );
    ~VlkPipeline();

    NoCopy( VlkPipeline );

    operator VkPipeline() const { return m_pipeline; }

private:
    VkPipeline m_pipeline;
    VkDevice m_device;
};
