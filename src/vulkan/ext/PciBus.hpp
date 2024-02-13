#pragma once

#include <memory>
#include <stdint.h>
#include <vulkan/vulkan.h>

class GpuDevice;

VkPhysicalDevice GetPhysicalDeviceForPciBus( uint16_t domain, uint8_t bus, uint8_t dev, uint8_t func );
std::shared_ptr<GpuDevice> GetGpuDeviceForPhysicalDevice( VkPhysicalDevice dev );
