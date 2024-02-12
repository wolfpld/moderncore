#include <vulkan/vulkan.h>

#include "PciBus.hpp"
#include "server/GpuDevice.hpp"
#include "server/Server.hpp"

std::shared_ptr<GpuDevice> GetGpuForPciBus( uint16_t domain, uint8_t bus, uint8_t dev, uint8_t func )
{
    auto& gpus = Server::Instance().Gpus();

    for( auto& gpu : gpus )
    {
        VkPhysicalDevicePCIBusInfoPropertiesEXT pciBusInfo = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PCI_BUS_INFO_PROPERTIES_EXT };

        VkPhysicalDeviceProperties2 props = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
        props.pNext = &pciBusInfo;

        vkGetPhysicalDeviceProperties2( gpu->Device(), &props );

        if( pciBusInfo.pciDomain == domain && pciBusInfo.pciBus == bus && pciBusInfo.pciDevice == dev && pciBusInfo.pciFunction == func )
        {
            return gpu;
        }
    }

    return {};
}
