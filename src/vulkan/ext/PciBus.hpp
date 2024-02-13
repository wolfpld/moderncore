#pragma once

#include <stdint.h>
#include <vulkan/vulkan.h>

VkPhysicalDevice GetPhysicalDeviceForPciBus( uint16_t domain, uint8_t bus, uint8_t dev, uint8_t func );
