#include <vector>

#include "VlkInfo.hpp"
#include "VlkSwapchain.hpp"
#include "../util/Logs.hpp"

VlkSwapchain::VlkSwapchain( VkPhysicalDevice physicalDevice, VkSurfaceKHR surface )
    : m_properties( physicalDevice, surface )
{
#ifdef DEBUG
    PrintSwapchainProperties( m_properties );
#endif
}

VlkSwapchain::~VlkSwapchain()
{
}
