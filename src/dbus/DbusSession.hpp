#ifndef __DBUSSESSION_HPP__
#define __DBUSSESSION_HPP__

#include <functional>
#include <memory>
#include <systemd/sd-bus.h>
#include <vector>

#include "DbusMessage.hpp"
#include "../util/NoCopy.hpp"

class DbusSession
{
public:
    DbusSession();
    ~DbusSession();

    NoCopy( DbusSession );

    DbusMessage Call( const char* dst, const char* path, const char* iface, const char* member, const char* sig = nullptr, ... );
    bool MatchSignal( const char* sender, const char* path, const char* iface, const char* member, std::function<int(DbusMessage)> callback );

    operator bool() const { return m_bus; }

private:
    sd_bus* m_bus;
    std::vector<std::unique_ptr<std::function<int(DbusMessage)>>> m_callbacks;
};

#endif
