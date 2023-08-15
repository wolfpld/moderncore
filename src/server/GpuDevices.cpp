#include <algorithm>
#include <assert.h>
#include <inttypes.h>
#include <vulkan/vk_enum_string_helper.h>

#include "GpuDevices.hpp"
#include "../util/Logs.hpp"
#include "../vulkan/VlkDevice.hpp"
#include "../vulkan/VlkInstance.hpp"
#include "../vulkan/VlkPhysicalDevice.hpp"

GpuDevices::GpuDevices( VlkInstance& instance )
    : m_instance( instance )
{
}

GpuDevices::~GpuDevices()
{
    for( auto& gpu : m_gpus ) vkDeviceWaitIdle( *gpu.device );
}

const std::shared_ptr<VlkDevice>& GpuDevices::Get( VkPhysicalDevice physDev ) const
{
    for( auto& gpu : m_gpus )
    {
        if( gpu.physDev == physDev ) return gpu.device;
    }

    static std::shared_ptr<VlkDevice> nullDevice;
    return nullDevice;
}

static void PrintPhysicalDeviceInfo( VkPhysicalDevice physDev )
{
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties( physDev, &properties );

    uint32_t count;
    vkEnumerateDeviceExtensionProperties( physDev, nullptr, &count, nullptr );
    std::vector<VkExtensionProperties> exts( count );
    vkEnumerateDeviceExtensionProperties( physDev, nullptr, &count, exts.data() );

    mclog( LogLevel::Info, "Using Vulkan physical device '%s'", properties.deviceName );
    mclog( LogLevel::Info, "  API version: %" PRIu32 ".%" PRIu32 ".%" PRIu32, VK_API_VERSION_MAJOR( properties.apiVersion ), VK_API_VERSION_MINOR( properties.apiVersion ), VK_API_VERSION_PATCH( properties.apiVersion ) );
    mclog( LogLevel::Info, "  Driver version: %" PRIu32 ".%" PRIu32 ".%" PRIu32, VK_API_VERSION_MAJOR( properties.driverVersion ), VK_API_VERSION_MINOR( properties.driverVersion ), VK_API_VERSION_PATCH( properties.driverVersion ) );

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
    mclog( LogLevel::Info, "  Device type: %s", devType );

    mclog( LogLevel::Info, "  Max 2D texture size: %" PRIu32, properties.limits.maxImageDimension2D );

    VlkPhysicalDevice phys( physDev );
    auto& qfp = phys.GetQueueFamilyProperties();
    mclog( LogLevel::Info, "  Number of queue families: %zu", qfp.size() );

    for( size_t j=0; j<qfp.size(); j++ )
    {
        mclog( LogLevel::Info, "  Queue family %zu:", j );
        mclog( LogLevel::Info, "    Count: %" PRIu32, qfp[j].queueCount );
        mclog( LogLevel::Info, "    Timestamp valid bits: %" PRIu32, qfp[j].timestampValidBits );
        mclog( LogLevel::Info, "    Capabilities:" );
        if( qfp[j].queueFlags & VK_QUEUE_GRAPHICS_BIT ) mclog( LogLevel::Info, "      graphic" );
        if( qfp[j].queueFlags & VK_QUEUE_COMPUTE_BIT ) mclog( LogLevel::Info, "      compute" );
        if( qfp[j].queueFlags & VK_QUEUE_TRANSFER_BIT ) mclog( LogLevel::Info, "      transfer" );
        if( qfp[j].queueFlags & VK_QUEUE_SPARSE_BINDING_BIT ) mclog( LogLevel::Info, "      sparse" );
        if( qfp[j].queueFlags & VK_QUEUE_PROTECTED_BIT ) mclog( LogLevel::Info, "      protected" );
        if( qfp[j].queueFlags & VK_QUEUE_VIDEO_DECODE_BIT_KHR ) mclog( LogLevel::Info, "      video decode" );
    }
}

static void PrintQueueConfig( const VlkDevice& device )
{
    mclog( LogLevel::Info, "  Queue configuration:" );

    {
        auto q = device.GetQueueInfo( QueueType::Graphic );
        if( q.idx >= 0 )
        {
            mclog( LogLevel::Info, "    Graphics queue family: %i", q.idx );
            if( q.shareCompute ) mclog( LogLevel::Info, "      <share compute>" );
            if( q.shareTransfer ) mclog( LogLevel::Info, "      <share transfer>" );
            if( q.sharePresent ) mclog( LogLevel::Info, "      <share present>" );
        }
        else
        {
            mclog( LogLevel::Info, "    Graphics queue family not present" );
        }
    }

    {
        auto q = device.GetQueueInfo( QueueType::Compute );
        if( q.idx >= 0 )
        {
            mclog( LogLevel::Info, "    Compute queue family: %i", q.idx );
            if( q.shareGraphic ) mclog( LogLevel::Info, "      <share graphic>" );
            if( q.shareTransfer ) mclog( LogLevel::Info, "      <share transfer>" );
            if( q.sharePresent ) mclog( LogLevel::Info, "      <share present>" );
        }
        else
        {
            mclog( LogLevel::Info, "    Compute queue family not present" );
        }
    }

    {
        auto q = device.GetQueueInfo( QueueType::Transfer );
        if( q.idx >= 0 )
        {
            mclog( LogLevel::Info, "    Transfer queue family: %i", q.idx );
            if( q.shareGraphic ) mclog( LogLevel::Info, "      <share graphic>" );
            if( q.shareCompute ) mclog( LogLevel::Info, "      <share compute>" );
            if( q.sharePresent ) mclog( LogLevel::Info, "      <share present>" );
        }
        else
        {
            mclog( LogLevel::Info, "    Transfer queue family not present" );
        }
    }

    {
        auto q = device.GetQueueInfo( QueueType::Present );
        if( q.idx >= 0 )
        {
            mclog( LogLevel::Info, "    Present queue family: %i", q.idx );
            if( q.shareGraphic ) mclog( LogLevel::Info, "      <share graphic>" );
            if( q.shareCompute ) mclog( LogLevel::Info, "      <share compute>" );
            if( q.shareTransfer ) mclog( LogLevel::Info, "      <share transfer>" );
        }
        else
        {
            mclog( LogLevel::Info, "    Present queue family not present" );
        }
    }
}

std::shared_ptr<VlkDevice> GpuDevices::Add( VkPhysicalDevice physDev, VkSurfaceKHR surface )
{
    assert( std::none_of( m_gpus.begin(), m_gpus.end(), [physDev]( const Gpu& gpu ) { return gpu.physDev == physDev; } ) );

    auto device = std::make_shared<VlkDevice>( m_instance, physDev, VlkDevice::RequireGraphic | VlkDevice::RequirePresent, surface );
    m_gpus.push_back( { physDev, device } );

    PrintPhysicalDeviceInfo( physDev );
    PrintQueueConfig( *device );

    return device;
}
