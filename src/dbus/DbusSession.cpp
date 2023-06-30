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
