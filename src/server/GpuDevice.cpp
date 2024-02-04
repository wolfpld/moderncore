#include <algorithm>
#include <assert.h>

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

void GpuDevice::AddConnector( std::shared_ptr<Connector> connector )
{
    m_connectors.emplace_back( std::move( connector ) );
}

void GpuDevice::RemoveConnector( const std::shared_ptr<Connector>& connector )
{
    auto it = std::find( m_connectors.begin(), m_connectors.end(), connector );
    assert( it != m_connectors.end() );
    m_connectors.erase( it );
}
