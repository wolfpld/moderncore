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
