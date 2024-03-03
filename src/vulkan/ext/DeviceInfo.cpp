#include <inttypes.h>
#include <vector>

#include "DeviceInfo.hpp"
#include "util/Logs.hpp"
#include "vulkan/VlkDevice.hpp"
#include "vulkan/VlkPhysicalDevice.hpp"

void PrintPhysicalDeviceInfo( const VlkPhysicalDevice& physDev )
{
    auto& properties = physDev.GetProperties();

    uint32_t count;
    vkEnumerateDeviceExtensionProperties( physDev, nullptr, &count, nullptr );
    std::vector<VkExtensionProperties> exts( count );
    vkEnumerateDeviceExtensionProperties( physDev, nullptr, &count, exts.data() );

    const char* devType;
    switch( properties.deviceType )
    {
    case VK_PHYSICAL_DEVICE_TYPE_OTHER: devType = "Other"; break;
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: devType = "Integrated GPU"; break;
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: devType = "Discrete GPU"; break;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: devType = "Virtual GPU"; break;
    case VK_PHYSICAL_DEVICE_TYPE_CPU: devType = "CPU"; break;
    default: devType = "Unknown"; break;
    };

    auto& qfp = physDev.GetQueueFamilyProperties();

    mclog( LogLevel::Info, "Using Vulkan physical device '%s'", properties.deviceName );
    mclog( LogLevel::Info, "  API: %" PRIu32 ".%" PRIu32 ".%" PRIu32 ", driver: %" PRIu32 ".%" PRIu32 ".%" PRIu32 ", type: %s, max tex: %" PRIu32 ", queue families: %zu",
        VK_API_VERSION_MAJOR( properties.apiVersion ), VK_API_VERSION_MINOR( properties.apiVersion ), VK_API_VERSION_PATCH( properties.apiVersion ),
        VK_API_VERSION_MAJOR( properties.driverVersion ), VK_API_VERSION_MINOR( properties.driverVersion ), VK_API_VERSION_PATCH( properties.driverVersion ),
        devType, properties.limits.maxImageDimension2D, qfp.size() );

    for( size_t j=0; j<qfp.size(); j++ )
    {
        mclog( LogLevel::Info, "    Queue family %zu, count: %" PRIu32 ", %" PRIu32 "-bit timestamp, caps:%s%s%s%s%s%s", j, qfp[j].queueCount, qfp[j].timestampValidBits,
            (qfp[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) ? " graphic" : "",
            (qfp[j].queueFlags & VK_QUEUE_COMPUTE_BIT) ? " compute" : "",
            (qfp[j].queueFlags & VK_QUEUE_TRANSFER_BIT) ? " transfer" : "",
            (qfp[j].queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) ? " sparse" : "",
            (qfp[j].queueFlags & VK_QUEUE_PROTECTED_BIT) ? " protected" : "",
            (qfp[j].queueFlags & VK_QUEUE_VIDEO_DECODE_BIT_KHR) ? " video_decode" : "" );
    }
}

void PrintQueueConfig( const VlkDevice& device )
{
    mclog( LogLevel::Info, "  Using queue configuration:" );

    {
        auto q = device.GetQueueInfo( QueueType::Graphic );
        if( q.idx >= 0 )
        {
            mclog( LogLevel::Info, "    Graphics queue family: %i%s%s%s%s", q.idx,
                q.shareCompute || q.shareTransfer || q.sharePresent ? ", sharing:" : "",
                q.shareCompute ? " compute" : "",
                q.shareTransfer ? " transfer" : "",
                q.sharePresent ? " present" : "" );
        }
        else
        {
            mclog( LogLevel::Debug, "    Graphics queue family not present" );
        }
    }

    {
        auto q = device.GetQueueInfo( QueueType::Compute );
        if( q.idx >= 0 )
        {
            mclog( LogLevel::Info, "    Compute queue family: %i%s%s%s%s", q.idx,
                q.shareGraphic || q.shareTransfer || q.sharePresent ? ", sharing:" : "",
                q.shareGraphic ? " graphic" : "",
                q.shareTransfer ? " transfer" : "",
                q.sharePresent ? " present" : "" );
        }
        else
        {
            mclog( LogLevel::Debug, "    Compute queue family not present" );
        }
    }

    {
        auto q = device.GetQueueInfo( QueueType::Transfer );
        if( q.idx >= 0 )
        {
            mclog( LogLevel::Info, "    Transfer queue family: %i%s%s%s%s", q.idx,
                q.shareGraphic || q.shareCompute || q.sharePresent ? ", sharing:" : "",
                q.shareGraphic ? " graphic" : "",
                q.shareCompute ? " compute" : "",
                q.sharePresent ? " present" : "" );
        }
        else
        {
            mclog( LogLevel::Debug, "    Transfer queue family not present" );
        }
    }

    {
        auto q = device.GetQueueInfo( QueueType::Present );
        if( q.idx >= 0 )
        {
            mclog( LogLevel::Info, "    Present queue family: %i%s%s%s%s", q.idx,
                q.shareGraphic || q.shareCompute || q.shareTransfer ? ", sharing:" : "",
                q.shareGraphic ? " graphic" : "",
                q.shareCompute ? " compute" : "",
                q.shareTransfer ? " transfer" : "" );
        }
        else
        {
            mclog( LogLevel::Debug, "    Present queue family not present" );
        }
    }
}
