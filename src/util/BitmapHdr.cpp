#include <cmath>
#include <stb_image_resize2.h>

#include "Bitmap.hpp"
#include "BitmapHdr.hpp"
#include "Panic.hpp"
#include "TaskDispatch.hpp"

BitmapHdr::BitmapHdr( uint32_t width, uint32_t height, Colorspace colorspace )
    : m_width( width )
    , m_height( height )
    , m_data( new float[width*height*4] )
    , m_colorspace( colorspace )
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

std::unique_ptr<Bitmap> BitmapHdr::Tonemap( ToneMap::Operator op )
{
    CheckPanic( m_colorspace == Colorspace::BT709, "Tone mapping requires BT.709 colorspace" );
    auto bmp = std::make_unique<Bitmap>( m_width, m_height );
    ToneMap::Process( op, (uint32_t*)bmp->Data(), m_data, m_width * m_height );
    return bmp;
}
