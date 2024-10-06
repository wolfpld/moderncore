#include "GpuDevice.hpp"
#include "vulkan/ext/DeviceInfo.hpp"
#include "vulkan/VlkDevice.hpp"

GpuDevice::GpuDevice( VlkInstance& instance, const std::shared_ptr<VlkPhysicalDevice>& physDev )
    : m_device( std::make_shared<VlkDevice>( instance, physDev, VlkDevice::RequireGraphic | VlkDevice::RequirePresent ) )
{
    PrintPhysicalDeviceInfo( *physDev );
    PrintQueueConfig( *m_device );
}

GpuDevice::~GpuDevice()
{
    vkDeviceWaitIdle( *m_device );
}

bool GpuDevice::IsPresentSupported( VkSurfaceKHR surface ) const
{
    auto queueIdx = m_device->GetQueueInfo( QueueType::Graphic ).idx;
    VkBool32 presentSupport = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR( *m_device, queueIdx, surface, &presentSupport );
    return presentSupport == VK_TRUE;
}
