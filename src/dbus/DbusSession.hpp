#ifndef __DBUSSESSION_HPP__
#define __DBUSSESSION_HPP__

#include "../util/NoCopy.hpp"

struct sd_bus;

class DbusSession
{
public:
    DbusSession();
    ~DbusSession();

    NoCopy( DbusSession );

    operator bool() const { return m_bus; }

private:
    sd_bus* m_bus;
};

#endif
