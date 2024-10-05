#pragma once

#include <memory>
#include <stdint.h>
#include <vector>
#include <vulkan/vulkan.h>

class GpuDevice;
class VlkPhysicalDevice;

std::shared_ptr<VlkPhysicalDevice> GetPhysicalDeviceForPciBus( uint16_t domain, uint8_t bus, uint8_t dev, uint8_t func );
std::shared_ptr<GpuDevice> GetGpuDeviceForPhysicalDevice( VlkPhysicalDevice& dev, const std::vector<std::shared_ptr<GpuDevice>>& gpus );
