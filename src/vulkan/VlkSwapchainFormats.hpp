#pragma once

#include <array>
#include <vulkan/vulkan.h>

constexpr inline std::array SdrSwapchainFormats = {
    VkSurfaceFormatKHR { VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR },
    VkSurfaceFormatKHR { VK_FORMAT_R8G8B8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR },
};
