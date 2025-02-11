#pragma once

#include <vulkan/vulkan.h>

#include "VlkBase.hpp"

struct wl_display;
struct wl_surface;

class VlkSurface : public VlkBase
{
public:
    VlkSurface( VkInstance instance, wl_display* display, wl_surface* surface );
    ~VlkSurface();

    operator VkSurfaceKHR() const { return m_surface; }

private:
    VkSurfaceKHR m_surface;
    VkInstance m_instance;
};
