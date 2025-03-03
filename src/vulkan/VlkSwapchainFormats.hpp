#pragma once

#include <array>
#include <vulkan/vulkan.h>

constexpr inline std::array SdrSwapchainFormats = {
    VkSurfaceFormatKHR { VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR },
    VkSurfaceFormatKHR { VK_FORMAT_R8G8B8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR },
};

constexpr inline std::array HdrSwapchainFormats = {
    VkSurfaceFormatKHR { VK_FORMAT_R16G16B16A16_SFLOAT, VK_COLOR_SPACE_BT709_LINEAR_EXT },
};
