#include <systemd/sd-bus.h>

#include "DbusMessage.hpp"
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

bool DbusSession::MatchSignal( const char* sender, const char* path, const char* iface, const char* member, std::function<int(DbusMessage)> callback )
{
    m_callbacks.emplace_back( std::make_unique<std::function<int(DbusMessage)>>( std::move( callback ) ) );

    auto res = sd_bus_match_signal( m_bus, nullptr, sender, path, iface, member, []( sd_bus_message* msg, void* userdata, sd_bus_error* ) -> int
    {
        auto cb = (const std::function<int(DbusMessage)>*)userdata;
        return (*cb)( DbusMessage( msg ) );
    }, m_callbacks.back().get() );

    if( res < 0 )
    {
        mclog( LogLevel::Error, "Failed to match signal %s.%s: %s", iface, member, strerror( -res ) );
        return false;
    }
    return true;
}

bool DbusSession::GetProperty( const char* dst, const char* path, const char* iface, const char* member, bool& out )
{
    sd_bus_error err = SD_BUS_ERROR_NULL;
    int val;
    if( sd_bus_get_property_trivial( m_bus, dst, path, iface, member, &err, 'b', &val ) < 0 )
    {
        mclog( LogLevel::Error, "Failed to get property %s.%s: %s", iface, member, err.message );
        sd_bus_error_free( &err );
        return false;
    }
    out = val;
    return true;
}
