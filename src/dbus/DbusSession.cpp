#include <systemd/sd-bus.h>

#include "DbusSession.hpp"
#include "../util/Logs.hpp"

DbusSession::DbusSession()
    : m_bus( nullptr )
{
    int ret = sd_bus_default_system( &m_bus );
    if( ret < 0 )
    {
        mclog( LogLevel::Error, "Failed to connect to system bus: %s", strerror( -ret ) );
    }
}

DbusSession::~DbusSession()
{
    sd_bus_unref( m_bus );
}

DbusMessage DbusSession::Call( const char* dst, const char* path, const char* iface, const char* member, const char* sig, ... )
{
    sd_bus_error err = SD_BUS_ERROR_NULL;
    sd_bus_message* msg = nullptr;

    va_list ap;
    va_start( ap, sig );

    auto res = sd_bus_call_methodv( m_bus, dst, path, iface, member, &err, &msg, sig, ap );
    va_end( ap );

    if( res < 0 )
    {
        mclog( LogLevel::Error, "Failed to call method %s.%s: %s", iface, member, err.message );
        sd_bus_error_free( &err );
        return {};
    }

    return DbusMessage( msg );
}
