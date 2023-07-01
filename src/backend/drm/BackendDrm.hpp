#ifndef __BACKENDDRM_HPP__
#define __BACKENDDRM_HPP__

#include "../Backend.hpp"

class BackendDrm : public Backend
{
public:
    explicit BackendDrm( VkInstance vkInstance );
    ~BackendDrm() override;

    void VulkanInit( VlkDevice& device, VkRenderPass renderPass, const VlkSwapchain& swapchain ) override;

    void Run( const std::function<void()>& render ) override;
    void Stop() override;

    void RenderCursor( VkCommandBuffer cmdBuf, CursorLogic& cursorLogic ) override;

    operator VkSurfaceKHR() const override;

private:

};

#endif
