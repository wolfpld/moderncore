#ifndef __DBUSSESSION_HPP__
#define __DBUSSESSION_HPP__

#include "DbusMessage.hpp"
#include "../util/NoCopy.hpp"

struct sd_bus;

class DbusSession
{
public:
    DbusSession();
    ~DbusSession();

    NoCopy( DbusSession );

    DbusMessage Call( const char* dst, const char* path, const char* iface, const char* member, const char* sig = nullptr, ... );

    operator bool() const { return m_bus; }

private:
    sd_bus* m_bus;
};

#endif
