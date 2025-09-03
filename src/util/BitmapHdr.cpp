#include <cmath>
#include <stb_image_resize2.h>
#include <string.h>

#include "Bitmap.hpp"
#include "BitmapHdr.hpp"
#include "Panic.hpp"
#include "TaskDispatch.hpp"

BitmapHdr::BitmapHdr( uint32_t width, uint32_t height, Colorspace colorspace, int orientation )
    : m_width( width )
    , m_height( height )
    , m_data( new float[width*height*4] )
    , m_colorspace( colorspace )
    , m_orientation( orientation )
{
}

BitmapHdr::~BitmapHdr()
{
    delete[] m_data;
}

void BitmapHdr::Resize( uint32_t width, uint32_t height, TaskDispatch* td )
{
    auto newData = new float[width*height*4];
    STBIR_RESIZE resize;
    stbir_resize_init( &resize, m_data, m_width, m_height, 0, newData, width, height, 0, STBIR_RGBA, STBIR_TYPE_FLOAT );
    stbir_set_non_pm_alpha_speed_over_quality( &resize, 1 );
    if( td )
    {
        auto threads = td->NumWorkers() + 1;
        threads = stbir_build_samplers_with_splits( &resize, threads );
        for( size_t i=0; i<threads; i++ )
        {
            td->Queue( [i, &resize, threads] {
                stbir_resize_extended_split( &resize, i, 1 );
            } );
        }
        td->Sync();
        stbir_free_samplers( &resize );
    }
    else
    {
        stbir_resize_extended( &resize );
    }
    delete[] m_data;
    m_data = newData;
    m_width = width;
    m_height = height;
}

std::unique_ptr<BitmapHdr> BitmapHdr::ResizeNew( uint32_t width, uint32_t height, TaskDispatch* td ) const
{
    auto ret = std::make_unique<BitmapHdr>( width, height, m_colorspace );
    STBIR_RESIZE resize;
    stbir_resize_init( &resize, m_data, m_width, m_height, 0, ret->m_data, width, height, 0, STBIR_RGBA, STBIR_TYPE_FLOAT );
    stbir_set_non_pm_alpha_speed_over_quality( &resize, 1 );
    if( td )
    {
        auto threads = td->NumWorkers() + 1;
        threads = stbir_build_samplers_with_splits( &resize, threads );
        for( size_t i=0; i<threads; i++ )
        {
            td->Queue( [i, &resize, threads] {
                stbir_resize_extended_split( &resize, i, 1 );
            } );
        }
        td->Sync();
        stbir_free_samplers( &resize );
    }
    else
    {
        stbir_resize_extended( &resize );
    }
    return ret;
}

void BitmapHdr::SetAlpha( float alpha )
{
    auto ptr = m_data;
    size_t sz = m_width * m_height;

    ptr += 3;
    while( sz-- )
    {
        memcpy( ptr, &alpha, sizeof( float ) );
        ptr += 4;
    }
}

void BitmapHdr::NormalizeOrientation()
{
    if( m_orientation <= 1 ) return;

    switch( m_orientation )
    {
    case 2:
        FlipHorizontal();
        break;
    case 3:
        Rotate180();
        break;
    case 4:
        FlipVertical();
        break;
    case 5:
        Rotate270();
        FlipVertical();
        break;
    case 6:
        Rotate90();
        break;
    case 7:
        Rotate90();
        FlipVertical();
        break;
    case 8:
        Rotate270();
        break;
    default:
        Panic( "Invalid orientation value!" );
    }

    m_orientation = 1;
}

void BitmapHdr::FlipVertical()
{
    auto ptr1 = m_data;
    auto ptr2 = m_data + ( m_height - 1 ) * m_width * 4;
    auto tmp = alloca( m_width * 4 * 4 );

    for( uint32_t y=0; y<m_height/2; y++ )
    {
        memcpy( tmp, ptr1, m_width * 4 * 4 );
        memcpy( ptr1, ptr2, m_width * 4 * 4 );
        memcpy( ptr2, tmp, m_width * 4 * 4 );
        ptr1 += m_width * 4;
        ptr2 -= m_width * 4;
    }
}

void BitmapHdr::FlipHorizontal()
{
    auto ptr = m_data;

    for( uint32_t y=0; y<m_height; y++ )
    {
        auto ptr1 = ptr;
        auto ptr2 = ptr + ( m_width - 1 ) * 4;

        for( uint32_t x=0; x<m_width/2; x++ )
        {
            uint32_t tmp[4];
            memcpy( tmp, ptr1, 4 * 4 );
            memcpy( ptr1, ptr2, 4 * 4 );
            memcpy( ptr2, tmp, 4 * 4 );
            ptr1 += 4;
            ptr2 -= 4;
        }

        ptr += m_width * 4;
    }
}

void BitmapHdr::Rotate90()
{
    auto tmp = new float[m_width * m_height * 4];

    auto src = m_data;
    auto dst = tmp;

    for( uint32_t y=0; y<m_height; y++ )
    {
        for( uint32_t x=0; x<m_width; x++ )
        {
            memcpy( dst + ( x * m_height + m_height - y - 1 ) * 4, src + ( y * m_width + x ) * 4, 4 * 4 );
        }
    }

    delete[] m_data;
    m_data = tmp;
    std::swap( m_width, m_height );
}

void BitmapHdr::Rotate180()
{
    auto ptr1 = m_data;
    auto ptr2 = m_data + ( m_width * m_height - 1 ) * 4;

    for( uint32_t i=0; i<m_width * m_height / 2; i++ )
    {
        uint32_t tmp[4];
        memcpy( tmp, ptr1, 4 * 4 );
        memcpy( ptr1, ptr2, 4 * 4 );
        memcpy( ptr2, tmp, 4 * 4 );
        ptr1 += 4;
        ptr2 -= 4;
    }
}

void BitmapHdr::Rotate270()
{
    auto tmp = new float[m_width * m_height * 4];

    auto src = m_data;
    auto dst = tmp;

    for( uint32_t y=0; y<m_height; y++ )
    {
        for( uint32_t x=0; x<m_width; x++ )
        {
            memcpy( dst + ( ( m_width - x - 1 ) * m_height + y ) * 4, src + ( y * m_width + x ) * 4, 4 * 4 );
        }
    }

    delete[] m_data;
    m_data = tmp;
    std::swap( m_width, m_height );
}

std::unique_ptr<Bitmap> BitmapHdr::Tonemap( ToneMap::Operator op )
{
    CheckPanic( m_colorspace == Colorspace::BT709, "Tone mapping requires BT.709 colorspace" );
    auto bmp = std::make_unique<Bitmap>( m_width, m_height );
    ToneMap::Process( op, (uint32_t*)bmp->Data(), m_data, m_width * m_height );
    return bmp;
}
