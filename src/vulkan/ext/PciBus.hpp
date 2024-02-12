#pragma once

#include <memory>
#include <stdint.h>

class GpuDevice;

std::shared_ptr<GpuDevice> GetGpuForPciBus( uint16_t domain, uint8_t bus, uint8_t dev, uint8_t func );
