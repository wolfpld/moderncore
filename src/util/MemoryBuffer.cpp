#include "MemoryBuffer.hpp"

MemoryBuffer::MemoryBuffer( std::vector<char>&& buf )
    : m_buf( std::move( buf ) )
{
    m_data = m_buf.data();
    m_size = m_buf.size();
}
