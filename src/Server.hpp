#ifndef __SERVER_HPP__
#define __SERVER_HPP__

#include "Backend.hpp"
#include "Display.hpp"

class Server
{
public:
    Server();

private:
    Display m_dpy;
    Backend m_backend;
};

#endif
