#include <unistd.h>

#include "MemoryBuffer.hpp"

MemoryBuffer::MemoryBuffer( std::vector<char>&& buf )
    : m_buf( std::move( buf ) )
{
    m_data = m_buf.data();
    m_size = m_buf.size();
}

MemoryBuffer::MemoryBuffer( int fd )
{
    if( fd < 0 ) return;

    char buf[64*1024];
    while( true )
    {
        auto len = read( fd, buf, sizeof( buf ) );
        if( len <= 0 ) break;
        m_buf.insert( m_buf.end(), buf, buf + len );
    }
    close( fd );

    m_data = m_buf.data();
    m_size = m_buf.size();
}

std::string MemoryBuffer::AsString() const
{
    return { m_buf.data(), m_buf.size() };
}
