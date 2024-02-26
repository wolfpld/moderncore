#pragma once

#ifndef TRACY_ENABLE

#define ZoneVkDevice( device )

#else

#include "vulkan/VlkPhysicalDevice.hpp"

#define ZoneVkDevice( device ) { auto& properties = device->GetProperties(); ZoneText( properties.deviceName, strlen( properties.deviceName ) ); }

#endif
