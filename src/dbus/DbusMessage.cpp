#include <systemd/sd-bus.h>

#include "DbusMessage.hpp"

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
