#include "GpuDevice.hpp"
#include "vulkan/ext/DeviceInfo.hpp"

GpuDevice::GpuDevice( VkInstance instance, VkPhysicalDevice physDev )
    : m_device( instance, physDev, VlkDevice::RequireGraphic )
{
    PrintPhysicalDeviceInfo( physDev );
    PrintQueueConfig( m_device );
}

GpuDevice::~GpuDevice()
{
    vkDeviceWaitIdle( m_device );
}

bool GpuDevice::IsPresentSupported( VkSurfaceKHR surface ) const
{
    auto queueIdx = m_device.GetQueueInfo( QueueType::Graphic ).idx;
    VkBool32 presentSupport = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR( m_device, queueIdx, surface, &presentSupport );
    return presentSupport == VK_TRUE;
}
