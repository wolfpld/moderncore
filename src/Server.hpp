#ifndef __SERVER_HPP__
#define __SERVER_HPP__

#include "Allocator.hpp"
#include "Backend.hpp"
#include "Compositor.hpp"
#include "Display.hpp"
#include "Renderer.hpp"

class Server
{
public:
    Server();

private:
    Display m_dpy;
    Backend m_backend;
    Renderer m_renderer;
    Allocator m_allocator;
    Compositor m_compositor;
};

#endif
