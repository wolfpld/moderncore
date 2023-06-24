#ifndef __VLKFRAMEBUFFER_HPP__
#define __VLKFRAMEBUFFER_HPP__

#include <vulkan/vulkan.h>

#include "../util/NoCopy.hpp"

class VlkFramebuffer
{
public:
    VlkFramebuffer( VkDevice device, const VkFramebufferCreateInfo& createInfo );
    ~VlkFramebuffer();

    NoCopy( VlkFramebuffer );

    operator VkFramebuffer() const { return m_framebuffer; }

private:
    VkFramebuffer m_framebuffer;
    VkDevice m_device;
};

#endif
