#pragma once

#ifndef TRACY_ENABLE

#define ZoneVkDevice( device )

#else

#define ZoneVkDevice( device ) { VkPhysicalDeviceProperties properties; vkGetPhysicalDeviceProperties( device, &properties ); ZoneText( properties.deviceName, strlen( properties.deviceName ) ); }

#endif
