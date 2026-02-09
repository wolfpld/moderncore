#include <cmath>
#include <ImfStdIO.h>
#include <ImfRgbaFile.h>
#include <lcms2.h>
#include <stb_image_resize2.h>
#include <tracy/Tracy.hpp>

#if defined __F16C__
#  include <x86intrin.h>
#endif

#include "contrib/half.hpp"

#include "BitmapHdr.hpp"
#include "BitmapHdrHalf.hpp"
#include "Logs.hpp"
#include "Panic.hpp"
#include "TaskDispatch.hpp"

static void FloatToHalf( const float* src, half_float::half* dst, size_t sz )
{
    ZoneScoped;

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

void BitmapHdrHalf::Crop( uint32_t x, uint32_t y, uint32_t width, uint32_t height )
{
    CheckPanic( x + width <= m_width && y + height <= m_height, "Invalid crop" );

    auto data = new half_float::half[width*height*4];
    auto dst = data;
    auto src = m_data + ( y * m_width + x ) * 4;

    for( uint32_t i=0; i<height; i++ )
    {
        memcpy( dst, src, width * 4 * sizeof( half_float::half ) );
        src += m_width * 4;
        dst += width * 4;
    }

    delete[] m_data;
    m_data = data;
    m_width = width;
    m_height = height;
}

void BitmapHdrHalf::SetColorspace( Colorspace colorspace, TaskDispatch* td )
{
    if( m_colorspace == colorspace )
    {
        mclog( LogLevel::Warning, "Requested a no-op colorspace transform." );
        return;
    }

    ZoneScoped;

    cmsToneCurve* linear = cmsBuildGamma( nullptr, 1 );
    cmsToneCurve* linear3[3] = { linear, linear, linear };

    auto profile709 = cmsCreateRGBProfile( &white709, &primaries709, linear3 );
    auto profile2020 = cmsCreateRGBProfile( &white709, &primaries2020, linear3 );

    cmsHTRANSFORM transform;
    if( m_colorspace == Colorspace::BT2020 && colorspace == Colorspace::BT709 )
    {
        transform = cmsCreateTransform( profile2020, TYPE_RGBA_HALF_FLT, profile709, TYPE_RGBA_HALF_FLT, INTENT_PERCEPTUAL, cmsFLAGS_COPY_ALPHA );
    }
    else if( m_colorspace == Colorspace::BT709 && colorspace == Colorspace::BT2020 )
    {
        transform = cmsCreateTransform( profile709, TYPE_RGBA_HALF_FLT, profile2020, TYPE_RGBA_HALF_FLT, INTENT_PERCEPTUAL, cmsFLAGS_COPY_ALPHA );
    }
    else
    {
        Panic( "Invalid colorspace transform!" );
    }

    if( td )
    {
        auto ptr = m_data;
        auto sz = m_width * m_height;
        while( sz > 0 )
        {
            auto chunk = std::min<uint32_t>( sz, 16 * 1024 );
            td->Queue( [ptr, chunk, transform] {
                cmsDoTransform( transform, ptr, ptr, chunk );
            } );
            ptr += chunk * 4;
            sz -= chunk;
        }
        td->Sync();
    }
    else
    {
        cmsDoTransform( transform, m_data, m_data, m_width * m_height );
    }

    cmsDeleteTransform( transform );
    cmsCloseProfile( profile709 );
    cmsCloseProfile( profile2020 );
    cmsFreeToneCurve( linear );

    m_colorspace = colorspace;
}

bool BitmapHdrHalf::SaveExr( const char* path ) const
{
    Imf::RgbaOutputFile output( path, m_width, m_height, Imf::WRITE_RGBA );
    output.setFrameBuffer( (const Imf::Rgba*)m_data, 1, m_width );
    output.writePixels( m_height );
    return true;
}

bool BitmapHdrHalf::SaveExr( int fd ) const
{
    Imf::StdOSStream buf;
    Imf::Header hdr( m_width, m_height );
    Imf::RgbaOutputFile output( buf, hdr );
    output.setFrameBuffer( (const Imf::Rgba*)m_data, 1, m_width );
    output.writePixels( m_height );

    auto str = buf.str();
    auto data = str.data();
    auto sz = str.size();

    while( sz > 0 )
    {
        auto chunk = std::min( sz, size_t( 64 * 1024 ) );
        auto cnt = write( fd, data, chunk );
        if( cnt <= 0 ) return false;
        data += cnt;
        sz -= cnt;
    }

    return true;
}
