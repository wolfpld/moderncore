#pragma once

#ifndef TRACY_ENABLE

#define ZoneVkDevice( device )
#define ZoneVk( device, cmdbuf, name, active )

#else

#include "vulkan/VlkPhysicalDevice.hpp"

#define ZoneVkDevice( device ) { auto& properties = device->Properties(); ZoneText( properties.deviceName, strlen( properties.deviceName ) ); }

#define ZoneVk( device, cmdbuf, name, active ) \
    static constexpr tracy::SourceLocationData TracyConcat(__tracy_gpu_source_location,TracyLine) { name, TracyFunction, TracyFile, (uint32_t)TracyLine, 0 }; \
    tracy::VkCtxScope __tracyVkScope( device.GetTracyContext(), &TracyConcat(__tracy_gpu_source_location,TracyLine), cmdbuf, active );

#endif
