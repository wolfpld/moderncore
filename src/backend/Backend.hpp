#ifndef __BACKEND_HPP__
#define __BACKEND_HPP__

#include <functional>
#include <vulkan/vulkan.h>

class VlkDevice;
class VlkSwapchain;

class Backend
{
public:
    virtual ~Backend() = default;

    virtual void VulkanInit( const VlkDevice& device, VkRenderPass renderPass, const VlkSwapchain& swapchain ) = 0;

    virtual void Run( const std::function<void()>& render ) = 0;
    virtual void Stop() = 0;

    virtual void RenderCursor() = 0;

    virtual operator VkSurfaceKHR() const = 0;

protected:
    Backend() = default;
};

#endif
