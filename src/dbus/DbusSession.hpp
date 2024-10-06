#pragma once

#include <functional>

#include "DbusMessage.hpp"
#include "util/NoCopy.hpp"

struct sd_bus;

class DbusSession
{
public:
    DbusSession();
    ~DbusSession();

    NoCopy( DbusSession );

    DbusMessage Call( const char* dst, const char* path, const char* iface, const char* member, const char* sig = nullptr, ... );
    bool MatchSignal( const char* sender, const char* path, const char* iface, const char* member, std::function<int(DbusMessage)> callback );

    bool GetProperty( const char* dst, const char* path, const char* iface, const char* member, bool& out );

    operator bool() const;
};
