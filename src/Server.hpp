#ifndef __SERVER_HPP__
#define __SERVER_HPP__

#include "plumbing/Allocator.hpp"
#include "plumbing/Backend.hpp"
#include "plumbing/Compositor.hpp"
#include "plumbing/DataDeviceManager.hpp"
#include "plumbing/Display.hpp"
#include "plumbing/Output.hpp"
#include "plumbing/Renderer.hpp"
#include "plumbing/Scene.hpp"
#include "plumbing/Seat.hpp"
#include "plumbing/Subcompositor.hpp"
#include "plumbing/XdgShell.hpp"

class Server
{
public:
    Server();

private:
    Display m_dpy;
    BackendWlr m_backend;
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
