#include <assert.h>
#include <systemd/sd-bus.h>

#include "DbusMessage.hpp"
#include "util/Logs.hpp"

DbusMessage::DbusMessage()
    : m_msg( nullptr )
{
}

DbusMessage::DbusMessage( sd_bus_message* msg )
    : m_msg( msg )
{
}

DbusMessage::~DbusMessage()
{
    sd_bus_message_unref( m_msg );
}

DbusMessage::DbusMessage( DbusMessage&& other ) noexcept
    : m_msg( other.m_msg )
{
    other.m_msg = nullptr;
}

DbusMessage& DbusMessage::operator=( DbusMessage&& other ) noexcept
{
    sd_bus_message_unref( m_msg );
    m_msg = other.m_msg;
    other.m_msg = nullptr;
    return *this;
}

bool DbusMessage::Read( const char* sig, ... )
{
    assert( m_msg );

    va_list ap;
    va_start( ap, sig );

    auto res = sd_bus_message_readv( m_msg, sig, ap );
    va_end( ap );

    if( res < 0 ) mclog( LogLevel::Error, "Failed to read message: %s", strerror( -res ) );
    return res >= 0;
}
