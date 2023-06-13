#ifndef __VLKPIPELINE_HPP__
#define __VLKPIPELINE_HPP__

#include <vulkan/vulkan.h>

class VlkPipeline
{
public:
    VlkPipeline( VkDevice device, const VkGraphicsPipelineCreateInfo& createInfo );
    ~VlkPipeline();

    operator VkPipeline() const { return m_pipeline; }

private:
    VkPipeline m_pipeline;
    VkDevice m_device;
};

#endif
