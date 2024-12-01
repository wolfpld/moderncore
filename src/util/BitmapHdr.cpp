#include "BitmapHdr.hpp"

BitmapHdr::BitmapHdr( uint32_t width, uint32_t height )
    : m_width( width )
    , m_height( height )
    , m_data( new float[width*height*4] )
{
}

BitmapHdr::~BitmapHdr()
{
    delete[] m_data;
}
