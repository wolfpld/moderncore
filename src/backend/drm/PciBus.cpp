#include <tracy/Tracy.hpp>

#include "PciBus.hpp"
#include "backend/GpuDevice.hpp"
#include "server/Server.hpp"
#include "vulkan/VlkInstance.hpp"
#include "vulkan/VlkPhysicalDevice.hpp"

std::shared_ptr<VlkPhysicalDevice> GetPhysicalDeviceForPciBus( uint16_t domain, uint8_t bus, uint8_t dev, uint8_t func )
{
    ZoneScoped;

    auto& devices = Server::Instance().VkInstance().QueryPhysicalDevices();
    for( auto& device : devices )
    {
        if( !device->HasPciBusInfo() ) continue;

        VkPhysicalDevicePCIBusInfoPropertiesEXT pciBusInfo = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PCI_BUS_INFO_PROPERTIES_EXT };

        VkPhysicalDeviceProperties2 props = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
        props.pNext = &pciBusInfo;

        vkGetPhysicalDeviceProperties2( *device, &props );

        if( pciBusInfo.pciDomain == domain && pciBusInfo.pciBus == bus && pciBusInfo.pciDevice == dev && pciBusInfo.pciFunction == func )
        {
            return device;
        }
    }

    return {};
}

std::shared_ptr<GpuDevice> GetGpuDeviceForPhysicalDevice( VlkPhysicalDevice& dev, const std::vector<std::shared_ptr<GpuDevice>>& gpus )
{
    ZoneScoped;

    for( auto& gpu : gpus )
    {
        if( *gpu->Device()->GetPhysicalDevice() == dev )
        {
            return gpu;
        }
    }
    return {};
}
