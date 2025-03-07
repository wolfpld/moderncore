#include <cmath>
#include <stb_image_resize2.h>

#include "contrib/half.hpp"

#include "BitmapHdr.hpp"
#include "BitmapHdrHalf.hpp"
#include "TaskDispatch.hpp"

BitmapHdrHalf::BitmapHdrHalf( const BitmapHdr& bmp )
    : m_width( bmp.Width() )
    , m_height( bmp.Height() )
    , m_data( new half_float::half[m_width*m_height*4] )
    , m_colorspace( bmp.GetColorspace() )
{
    auto src = bmp.Data();
    auto dst = m_data;
    auto sz = m_width * m_height * 4;
    while( sz-- > 0 )
    {
        *dst++ = half_float::half( *src++ );
    }
}

BitmapHdrHalf::BitmapHdrHalf( uint32_t width, uint32_t height, Colorspace colorspace )
    : m_width( width )
    , m_height( height )
    , m_data( new half_float::half[width*height*4] )
    , m_colorspace( colorspace )
{
}

BitmapHdrHalf::~BitmapHdrHalf()
{
    delete[] m_data;
}

void BitmapHdrHalf::Resize( uint32_t width, uint32_t height, TaskDispatch* td )
{
    auto newData = new half_float::half[width*height*4];
    STBIR_RESIZE resize;
    stbir_resize_init( &resize, m_data, m_width, m_height, 0, newData, width, height, 0, STBIR_RGBA, STBIR_TYPE_HALF_FLOAT );
    stbir_set_non_pm_alpha_speed_over_quality( &resize, 1 );
    if( td )
    {
        auto threads = td->NumWorkers();
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

std::unique_ptr<BitmapHdrHalf> BitmapHdrHalf::ResizeNew( uint32_t width, uint32_t height, TaskDispatch* td ) const
{
    auto ret = std::make_unique<BitmapHdrHalf>( width, height, m_colorspace );
    STBIR_RESIZE resize;
    stbir_resize_init( &resize, m_data, m_width, m_height, 0, ret->m_data, width, height, 0, STBIR_RGBA, STBIR_TYPE_HALF_FLOAT );
    stbir_set_non_pm_alpha_speed_over_quality( &resize, 1 );
    if( td )
    {
        auto threads = td->NumWorkers();
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
