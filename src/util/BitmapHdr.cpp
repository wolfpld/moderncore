#include <cmath>
#include <lcms2.h>
#include <stb_image_resize2.h>
#include <string.h>
#include <tracy/Tracy.hpp>

#if defined __F16C__
#  include <x86intrin.h>
#endif

#include "contrib/half.hpp"

#include "Bitmap.hpp"
#include "BitmapHdr.hpp"
#include "BitmapHdrHalf.hpp"
#include "Logs.hpp"
#include "Panic.hpp"
#include "Simd.hpp"
#include "TaskDispatch.hpp"

static void HalfToFloat( const half_float::half* src, float* dst, size_t sz )
{
    ZoneScoped;

#ifdef __F16C__
  #ifdef __AVX512F__
    while( sz >= 16 )
    {
        __m256i h = _mm256_loadu_si256( (__m256i*)src );
        __m512 f = _mm512_cvtph_ps( h );
        _mm512_storeu_ps( dst, f );
        src += 16;
        dst += 16;
        sz -= 16;
    }
  #endif
  #ifdef __AVX2__
    while( sz >= 8 )
    {
        __m128i h = _mm_loadu_si128( (__m128i*)src );
        __m256 f = _mm256_cvtph_ps( h );
        _mm256_storeu_ps( dst, f );
        src += 8;
        dst += 8;
        sz -= 8;
    }
  #endif
#endif

    while( sz-- > 0 )
    {
        *dst++ = *src++;
    }
}

BitmapHdr::BitmapHdr( const BitmapHdrHalf& bmp )
    : m_width( bmp.Width() )
    , m_height( bmp.Height() )
    , m_data( new float[m_width*m_height*4] )
    , m_colorspace( bmp.GetColorspace() )
{
    auto src = bmp.Data();
    auto dst = m_data;
    auto sz = m_width * m_height * 4;

    HalfToFloat( src, dst, sz );
}

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

void BitmapHdr::Crop( uint32_t x, uint32_t y, uint32_t width, uint32_t height )
{
    CheckPanic( x + width <= m_width && y + height <= m_height, "Invalid crop" );

    auto data = new float[width*height*4];
    auto dst = data;
    auto src = m_data + ( y * m_width + x ) * 4;

    for( uint32_t i=0; i<height; i++ )
    {
        memcpy( dst, src, width * 4 * sizeof( float ) );
        src += m_width * 4;
        dst += width * 4;
    }

    delete[] m_data;
    m_data = data;
    m_width = width;
    m_height = height;
}

void BitmapHdr::SetAlpha( float alpha )
{
    auto ptr = m_data;
    size_t sz = m_width * m_height;

#ifdef __AVX512F__
    while( sz >= 4 )
    {
        __m512 px = _mm512_loadu_ps( ptr );
        __m512 pxa = _mm512_mask_blend_ps( 0x8888, px, _mm512_set1_ps( alpha ) );
        _mm512_storeu_ps( ptr, pxa );
        ptr += 16;
        sz -= 4;
    }
#endif

#ifdef __AVX2__
    while( sz >= 2 )
    {
        __m256 px = _mm256_loadu_ps( ptr );
        __m256 pxa = _mm256_blend_ps( px, _mm256_set1_ps( alpha ), 0x88 );
        _mm256_storeu_ps( ptr, pxa );
        ptr += 8;
        sz -= 2;
    }
#endif

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

void BitmapHdr::SetColorspace( Colorspace colorspace, TaskDispatch* td )
{
    if( m_colorspace == colorspace )
    {
        mclog( LogLevel::Warning, "Requested a no-op colorspace transform." );
        return;
    }

    cmsToneCurve* linear = cmsBuildGamma( nullptr, 1 );
    cmsToneCurve* linear3[3] = { linear, linear, linear };

    auto profile709 = cmsCreateRGBProfile( &white709, &primaries709, linear3 );
    auto profile2020 = cmsCreateRGBProfile( &white709, &primaries2020, linear3 );

    cmsHTRANSFORM transform;
    if( m_colorspace == Colorspace::BT2020 && colorspace == Colorspace::BT709 )
    {
        transform = cmsCreateTransform( profile2020, TYPE_RGBA_FLT, profile709, TYPE_RGBA_FLT, INTENT_PERCEPTUAL, cmsFLAGS_COPY_ALPHA );
    }
    else if( m_colorspace == Colorspace::BT709 && colorspace == Colorspace::BT2020 )
    {
        transform = cmsCreateTransform( profile709, TYPE_RGBA_FLT, profile2020, TYPE_RGBA_FLT, INTENT_PERCEPTUAL, cmsFLAGS_COPY_ALPHA );
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
    ZoneScoped;
    CheckPanic( m_colorspace == Colorspace::BT709, "Tone mapping requires BT.709 colorspace" );
    auto bmp = std::make_unique<Bitmap>( m_width, m_height );
    ToneMap::Process( op, (uint32_t*)bmp->Data(), m_data, m_width * m_height );
    return bmp;
}
