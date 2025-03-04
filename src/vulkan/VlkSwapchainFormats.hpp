#pragma once

#include <array>
#include <vector>
#include <vulkan/vulkan.h>

constexpr inline std::array SdrSwapchainFormats = {
    VkSurfaceFormatKHR { VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR },
    VkSurfaceFormatKHR { VK_FORMAT_R8G8B8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR },
};

constexpr inline std::array HdrSwapchainFormats = {
    VkSurfaceFormatKHR { VK_FORMAT_R16G16B16A16_SFLOAT, VK_COLOR_SPACE_BT709_LINEAR_EXT },
};

template<size_t Size>
static inline VkSurfaceFormatKHR FindSwapchainFormat( const std::vector<VkSurfaceFormatKHR>& available, const std::array<VkSurfaceFormatKHR, Size>& supported )
{
    for( auto& format : available )
    {
        for( auto& valid : supported )
        {
            if( format.format == valid.format && format.colorSpace == valid.colorSpace )
            {
                return format;
            }
        }
    }

    return { VK_FORMAT_UNDEFINED };
}
