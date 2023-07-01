#include "BackendDrm.hpp"

BackendDrm::BackendDrm( VkInstance vkInstance )
{
}

BackendDrm::~BackendDrm()
{
}

void BackendDrm::VulkanInit( VlkDevice& device, VkRenderPass renderPass, const VlkSwapchain& swapchain )
{
}

void BackendDrm::Run( const std::function<void()>& render )
{
}

void BackendDrm::Stop()
{
}

void BackendDrm::RenderCursor( VkCommandBuffer cmdBuf, CursorLogic& cursorLogic )
{
}

BackendDrm::operator VkSurfaceKHR() const
{
}
