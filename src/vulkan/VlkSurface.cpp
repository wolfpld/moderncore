#include <vulkan/vulkan.h>
#include <vulkan/vulkan_wayland.h>

#include "VlkError.hpp"
#include "VlkSurface.hpp"

VlkSurface::VlkSurface( VkInstance instance, wl_display* display, wl_surface* surface )
    : m_instance( instance )
{
    const VkWaylandSurfaceCreateInfoKHR createInfo = {
        .sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
        .display = display,
        .surface = surface
    };
    VkVerify( vkCreateWaylandSurfaceKHR( instance, &createInfo, nullptr, &m_surface ) );
}

VlkSurface::~VlkSurface()
{
    vkDestroySurfaceKHR( m_instance, m_surface, nullptr );
}
