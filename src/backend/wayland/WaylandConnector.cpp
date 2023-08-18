#include "WaylandConnector.hpp"
#include "../../vulkan/VlkSwapchain.hpp"

WaylandConnector::WaylandConnector( const VlkDevice& device, VkSurfaceKHR surface )
    : m_swapchain( std::make_unique<VlkSwapchain>( device, surface ) )
{
}

WaylandConnector::~WaylandConnector()
{
}
