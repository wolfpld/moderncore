#include "PhysDevSel.hpp"
#include "vulkan/VlkPhysicalDevice.hpp"
#include "vulkan/VlkSwapchainFormats.hpp"
#include "vulkan/VlkSwapchainProperties.hpp"
#include "util/Logs.hpp"

namespace PhysDevSel
{

std::shared_ptr<VlkPhysicalDevice> PickBest( const std::vector<std::shared_ptr<VlkPhysicalDevice>>& list, VkSurfaceKHR presentSurface, int flags )
{
    std::shared_ptr<VlkPhysicalDevice> best;
    const bool preferIntegrated = flags & PreferIntegrated;
    int bestScore = 0;
    for( auto& dev : list )
    {
        if( flags != 0 || presentSurface != VK_NULL_HANDLE )
        {
            if( flags & RequireGraphic && !dev->IsGraphicCapable() ) continue;
            if( flags & RequireCompute && !dev->IsComputeCapable() ) continue;
            if( presentSurface != VK_NULL_HANDLE )
            {
                if( !dev->IsSwapchainCapable() ) continue;

                const auto numQueues = dev->QueueFamilyProperties().size();
                bool support = false;
                for( size_t i=0; i<numQueues; i++ )
                {
                    VkBool32 res;
                    vkGetPhysicalDeviceSurfaceSupportKHR( *dev, i, presentSurface, &res );
                    if( res )
                    {
                        support = true;
                        break;
                    }
                }
                if( !support ) continue;

                VlkSwapchainProperties swapchainProps( *dev, presentSurface );
                support = false;
                for( auto& format : swapchainProps.GetFormats() )
                {
                    for( auto& valid : SdrSwapchainFormats )
                    {
                        if( format.format == valid.format && format.colorSpace == valid.colorSpace )
                        {
                            support = true;
                            break;
                        }
                    }
                    if( support ) break;
                }
                if( !support ) continue;
            }
        }

        auto properties = dev->Properties();
        int score = properties.limits.maxImageDimension2D;
        switch( properties.deviceType )
        {
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            score += preferIntegrated ? 200000 : 100000;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            score += preferIntegrated ? 100000 : 200000;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_OTHER:
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
        default:
            break;
        }
        if( score > bestScore )
        {
            bestScore = score;
            best = dev;
        }

        mclog( LogLevel::Debug, "Vulkan physical device '%s' score %i", properties.deviceName, score );
    }
    return best;
}

}
