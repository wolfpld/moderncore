#include <string.h>
#include <utility>

#include "Bitmap.hpp"

Bitmap::Bitmap( uint32_t width, uint32_t height )
    : m_width( width )
    , m_height( height )
    , m_data( new uint8_t[width*height*4] )
{
}

Bitmap::~Bitmap()
{
    delete[] m_data;
}

Bitmap::Bitmap( Bitmap&& other ) noexcept
    : m_width( other.m_width )
    , m_height( other.m_height )
    , m_data( other.m_data )
{
    other.m_data = nullptr;
}

Bitmap& Bitmap::operator=( Bitmap&& other ) noexcept
{
    std::swap( m_width, other.m_width );
    std::swap( m_height, other.m_height );
    std::swap( m_data, other.m_data );
    return *this;
}
