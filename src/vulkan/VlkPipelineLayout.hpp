#ifndef __VLKPIPELINELAYOUT_HPP__
#define __VLKPIPELINELAYOUT_HPP__

#include <vulkan/vulkan.h>

class VlkPipelineLayout
{
public:
    VlkPipelineLayout( VkDevice device, const VkPipelineLayoutCreateInfo& createInfo );
    ~VlkPipelineLayout();

    operator VkPipelineLayout() const { return m_layout; }

private:
    VkPipelineLayout m_layout;
    VkDevice m_device;
};

#endif
