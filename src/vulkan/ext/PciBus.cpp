#include "PciBus.hpp"
#include "server/Server.hpp"
#include "vulkan/VlkInstance.hpp"

VkPhysicalDevice GetPhysicalDeviceForPciBus( uint16_t domain, uint8_t bus, uint8_t dev, uint8_t func )
{
    auto devices = Server::Instance().VkInstance().QueryPhysicalDevices();

    for( auto& device : devices )
    {
        VkPhysicalDevicePCIBusInfoPropertiesEXT pciBusInfo = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PCI_BUS_INFO_PROPERTIES_EXT };

        VkPhysicalDeviceProperties2 props = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
        props.pNext = &pciBusInfo;

        vkGetPhysicalDeviceProperties2( device, &props );

        if( pciBusInfo.pciDomain == domain && pciBusInfo.pciBus == bus && pciBusInfo.pciDevice == dev && pciBusInfo.pciFunction == func )
        {
            return device;
        }
    }

    return VK_NULL_HANDLE;
}
