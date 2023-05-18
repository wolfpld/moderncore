#ifndef __SERVER_HPP__
#define __SERVER_HPP__

#include "Allocator.hpp"
#include "Backend.hpp"
#include "Compositor.hpp"
#include "DataDeviceManager.hpp"
#include "Display.hpp"
#include "Output.hpp"
#include "Renderer.hpp"
#include "Subcompositor.hpp"

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
    Subcompositor m_subcompositor;
    DataDeviceManager m_ddm;
    Output m_output;
};

#endif
