#include "Server.hpp"

Server::Server()
    : m_backend( m_dpy )
    , m_renderer( m_backend, m_dpy )
    , m_allocator( m_backend, m_renderer )
{
}
