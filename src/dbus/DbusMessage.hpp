#ifndef __DBUSMESSAGE_HPP__
#define __DBUSMESSAGE_HPP__

#include "../util/NoCopy.hpp"

struct sd_bus_message;

class DbusMessage
{
public:
    DbusMessage();
    explicit DbusMessage( sd_bus_message* msg );
    ~DbusMessage();

    DbusMessage( DbusMessage&& ) noexcept;
    DbusMessage& operator=( DbusMessage&& ) noexcept;

    NoCopy( DbusMessage );

    operator bool() const { return m_msg; }

private:
    sd_bus_message* m_msg;
};

#endif
