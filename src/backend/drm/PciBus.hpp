#pragma once

#include <memory>
#include <stdint.h>

class VlkPhysicalDevice;

std::shared_ptr<VlkPhysicalDevice> GetPhysicalDeviceForPciBus( uint16_t domain, uint8_t bus, uint8_t dev, uint8_t func );
