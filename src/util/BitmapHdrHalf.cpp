#include <cmath>
#include <stb_image_resize2.h>

#if defined __F16C__
#  include <x86intrin.h>
#endif

#include "contrib/half.hpp"

#include "BitmapHdr.hpp"
#include "BitmapHdrHalf.hpp"
#include "TaskDispatch.hpp"

static void FloatToHalf( const float* src, half_float::half* dst, size_t sz )
{
#ifdef __F16C__
  #ifdef __AVX512F__
    while( sz >= 16 )
    {
        __m512 f = _mm512_loadu_ps( src );
        __m256i h = _mm512_cvtps_ph( f, _MM_FROUND_TO_NEAREST_INT );
        _mm256_storeu_si256( (__m256i*)dst, h );
        src += 16;
        dst += 16;
        sz -= 16;
    }
  #endif
  #ifdef __AVX2__
    while( sz >= 8 )
    {
        __m256 f = _mm256_loadu_ps( src );
        __m128i h = _mm256_cvtps_ph( f, _MM_FROUND_TO_NEAREST_INT );
        _mm_storeu_si128( (__m128i*)dst, h );
        src += 8;
        dst += 8;
        sz -= 8;
    }
  #endif
#endif

    while( sz-- > 0 )
    {
        *dst++ = half_float::half( *src++ );
    }
}

BitmapHdrHalf::BitmapHdrHalf( const BitmapHdr& bmp )
    : m_width( bmp.Width() )
    , m_height( bmp.Height() )
    , m_data( new half_float::half[m_width*m_height*4] )
    , m_colorspace( bmp.GetColorspace() )
{
    auto src = bmp.Data();
    auto dst = m_data;
    auto sz = m_width * m_height * 4;

    FloatToHalf( src, dst, sz );
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

std::unique_ptr<BitmapHdrHalf> BitmapHdrHalf::ResizeNew( uint32_t width, uint32_t height, TaskDispatch* td ) const
{
    auto ret = std::make_unique<BitmapHdrHalf>( width, height, m_colorspace );
    STBIR_RESIZE resize;
    stbir_resize_init( &resize, m_data, m_width, m_height, 0, ret->m_data, width, height, 0, STBIR_RGBA, STBIR_TYPE_HALF_FLOAT );
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
