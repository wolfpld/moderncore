#pragma once

#include <wayland-client.h>

#include "../../util/Logs.hpp"
#include "../../util/Panic.hpp"

template<typename T>
static inline T* RegistryBindImpl( wl_registry* reg, uint32_t name, const char* interfaceName, const wl_interface* interface, uint32_t version, uint32_t versionMin = 1, uint32_t versionMax = 1 )
{
    CheckPanic( version >= versionMin, "Wayland interface %s version %u is too old (minimum required is %u)", interfaceName, version, versionMin );
    if( version > versionMax )
    {
        mclog( LogLevel::Debug, "Wayland interface %s version %u is newer than supported version %u", interfaceName, version, versionMax );
        version = versionMax;
    }
    return (T*)wl_registry_bind( reg, name, interface, version );
}

// Two optional parameters: versionMin and versionMax.
#define RegistryBind( type, ... ) RegistryBindImpl<type>( reg, name, interface, &type ## _interface, version, ##__VA_ARGS__ )
