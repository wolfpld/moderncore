#include <algorithm>
#include <assert.h>
#include <tracy/Tracy.hpp>

#include "Connector.hpp"
#include "GpuDevice.hpp"
#include "util/Tracy.hpp"
#include "vulkan/ext/DeviceInfo.hpp"
#include "vulkan/VlkInstance.hpp"

GpuDevice::GpuDevice( VlkInstance& instance, VkPhysicalDevice physDev )
    : m_device( instance, physDev, VlkDevice::RequireGraphic | VlkDevice::RequirePresent )
{
    PrintPhysicalDeviceInfo( physDev );
    PrintQueueConfig( m_device );
}

GpuDevice::~GpuDevice()
{
    vkDeviceWaitIdle( m_device );
}

void GpuDevice::Render()
{
    ZoneScoped;
    ZoneVkDevice( m_device );

    for( auto& c : m_connectors ) c->Render();
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

bool GpuDevice::IsPresentSupported( VkSurfaceKHR surface ) const
{
    auto queueIdx = m_device.GetQueueInfo( QueueType::Graphic ).idx;
    VkBool32 presentSupport = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR( m_device, queueIdx, surface, &presentSupport );
    return presentSupport == VK_TRUE;
}
