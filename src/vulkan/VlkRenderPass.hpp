#ifndef __VLKRENDERPASS_HPP__
#define __VLKRENDERPASS_HPP__

#include <vulkan/vulkan.h>

#include "../util/NoCopy.hpp"

class VlkRenderPass
{
public:
    VlkRenderPass( VkDevice device, const VkRenderPassCreateInfo& createInfo );
    ~VlkRenderPass();

    NoCopy( VlkRenderPass );

    operator VkRenderPass() const { return m_renderPass; }

private:
    VkRenderPass m_renderPass;
    VkDevice m_device;
};

#endif
