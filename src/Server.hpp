#ifndef __SERVER_HPP__
#define __SERVER_HPP__

#include "Allocator.hpp"
#include "Backend.hpp"
#include "Compositor.hpp"
#include "DataDeviceManager.hpp"
#include "Display.hpp"
#include "Output.hpp"
#include "Renderer.hpp"
#include "Scene.hpp"
#include "Seat.hpp"
#include "Subcompositor.hpp"
#include "XdgShell.hpp"

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
    Scene m_scene;
    Output m_output;
    XdgShell m_xdgShell;
    Seat m_seat;
};

#endif
