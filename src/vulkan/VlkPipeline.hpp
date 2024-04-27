#pragma once

#include <vulkan/vulkan.h>

#include "VlkBase.hpp"
#include "util/NoCopy.hpp"

class VlkPipeline : public VlkBase
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
