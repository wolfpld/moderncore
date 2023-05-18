#include "Server.hpp"

Server::Server()
    : m_backend( m_dpy )
    , m_renderer( m_backend, m_dpy )
    , m_allocator( m_backend, m_renderer )
    , m_compositor( m_dpy, m_renderer )
    , m_subcompositor( m_dpy )
    , m_ddm( m_dpy )
{
}
