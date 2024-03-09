#pragma once

#ifndef TRACY_ENABLE

#define ZoneVkDevice( device )

#else

#include "vulkan/VlkPhysicalDevice.hpp"

#define ZoneVkDevice( device ) { auto& properties = device->Properties(); ZoneText( properties.deviceName, strlen( properties.deviceName ) ); }

#define ZoneVkNew( ctx, varname, cmdbuf, name, active ) { static constexpr tracy::SourceLocationData TracyConcat(__tracy_gpu_source_location,TracyLine) { name, TracyFunction, TracyFile, (uint32_t)TracyLine, 0 }; varname = new tracy::VkCtxScope( ctx, &TracyConcat(__tracy_gpu_source_location,TracyLine), cmdbuf, active ); }

#endif
