#include <systemd/sd-bus.h>
#include <tracy/Tracy.hpp>
#include <unistd.h>

#include "DbusMessage.hpp"
#include "DbusSession.hpp"
#include "util/Logs.hpp"

thread_local sd_bus* t_bus = nullptr;
thread_local int t_count = 0;
thread_local std::vector<std::unique_ptr<std::function<int(DbusMessage)>>> t_callbacks;

DbusSession::DbusSession()
{
    ZoneScoped;

    if( t_count++ == 0 )
    {
        int ret = sd_bus_default_system( &t_bus );
        if( ret < 0 )
        {
            mclog( LogLevel::Error, "Failed to connect to user bus: %s", strerror( -ret ) );
        }
    }
}

DbusSession::~DbusSession()
{
    if( --t_count == 0 )
    {
        if( !t_callbacks.empty() )
        {
            mclog( LogLevel::Debug, "Thread %i: clearing %zu DBus callbacks", gettid(), t_callbacks.size() );
            t_callbacks.clear();
        }

        sd_bus_unref( t_bus );
        t_bus = nullptr;
    }
}

DbusMessage DbusSession::Call( const char* dst, const char* path, const char* iface, const char* member, const char* sig, ... )
{
    ZoneScoped;
    ZoneTextF( "DBus call %s.%s", iface, member );

    sd_bus_error err = SD_BUS_ERROR_NULL;
    sd_bus_message* msg = nullptr;

    va_list ap;
    va_start( ap, sig );

    auto res = sd_bus_call_methodv( t_bus, dst, path, iface, member, &err, &msg, sig, ap );
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
    ZoneScoped;
    ZoneTextF( "DBus match %s.%s", iface, member );

    t_callbacks.emplace_back( std::make_unique<std::function<int(DbusMessage)>>( std::move( callback ) ) );

    auto res = sd_bus_match_signal( t_bus, nullptr, sender, path, iface, member, []( sd_bus_message* msg, void* userdata, sd_bus_error* ) -> int
    {
        auto cb = (const std::function<int(DbusMessage)>*)userdata;
        return (*cb)( DbusMessage( msg ) );
    }, t_callbacks.back().get() );

    if( res < 0 )
    {
        mclog( LogLevel::Error, "Failed to match signal %s.%s: %s", iface, member, strerror( -res ) );
        return false;
    }
    else
    {
        mclog( LogLevel::Debug, "Thread %i: Matched signal %s.%s", gettid(), iface, member );
    }
    return true;
}

bool DbusSession::GetProperty( const char* dst, const char* path, const char* iface, const char* member, bool& out )
{
    ZoneScoped;
    ZoneTextF( "DBus get property %s.%s", iface, member );

    sd_bus_error err = SD_BUS_ERROR_NULL;
    int val;
    if( sd_bus_get_property_trivial( t_bus, dst, path, iface, member, &err, 'b', &val ) < 0 )
    {
        mclog( LogLevel::Error, "Failed to get property %s.%s: %s", iface, member, err.message );
        sd_bus_error_free( &err );
        return false;
    }
    out = val;
    return true;
}

DbusSession::operator bool() const
{
    return t_bus != nullptr;
}
