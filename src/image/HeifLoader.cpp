#include <algorithm>
#include <cmath>
#include <libheif/heif.h>
#include <lcms2.h>
#include <pugixml.hpp>
#include <stb_image_resize2.h>
#include <string.h>
#include <vector>

#include "HeifLoader.hpp"
#include "util/Alloca.h"
#include "util/Bitmap.hpp"
#include "util/BitmapHdr.hpp"
#include "util/FileBuffer.hpp"
#include "util/FileWrapper.hpp"
#include "util/HdrEotf.hpp"
#include "util/Panic.hpp"
#include "util/Simd.hpp"
#include "util/TaskDispatch.hpp"
#include "util/Tonemapper.hpp"
#include "util/TransferCurve.hpp"

static bool SameCoordinate( float a, float b )
{
    // nclx coordinates are 16-bit fixed point ( 1/65535 ), so this absorbs ~33 quanta of storage
    // rounding while staying far below the narrowest gap between distinct primaries sets
    // ( P3-D65 red differs from BT.709 red by 0.04 )
    return fabsf( a - b ) < 0.0005f;
}

static bool SameRgbPrimaries( const cmsCIExyYTRIPLE& a, const cmsCIExyYTRIPLE& b )
{
    return SameCoordinate( a.Red.x, b.Red.x ) && SameCoordinate( a.Red.y, b.Red.y ) &&
        SameCoordinate( a.Green.x, b.Green.x ) && SameCoordinate( a.Green.y, b.Green.y ) &&
        SameCoordinate( a.Blue.x, b.Blue.x ) && SameCoordinate( a.Blue.y, b.Blue.y );
}

static bool SameWhite( const cmsCIExyY& a, const cmsCIExyY& b )
{
    return SameCoordinate( a.x, b.x ) && SameCoordinate( a.y, b.y );
}

static void ApplyGainMap( float* ptr, size_t sz, const float* gptr, float headroom )
{
    const float hadj = headroom - 1.f;

#if defined __SSE4_1__ && defined __FMA__
    __m128 hadj4 = _mm_set1_ps( hadj );
    while( sz >= 4 )
    {
        __m128 gain = _mm_loadu_ps( gptr );
        __m128 mul = _mm_fmadd_ps( hadj4, gain, _mm_set1_ps( 1.f ) );

        __m128 sh0 = _mm_shuffle_ps( mul, mul, _MM_SHUFFLE( 0, 0, 0, 0 ) );
        __m128 sh1 = _mm_shuffle_ps( mul, mul, _MM_SHUFFLE( 1, 1, 1, 1 ) );
        __m128 sh2 = _mm_shuffle_ps( mul, mul, _MM_SHUFFLE( 2, 2, 2, 2 ) );
        __m128 sh3 = _mm_shuffle_ps( mul, mul, _MM_SHUFFLE( 3, 3, 3, 3 ) );

  #ifdef __AVX2__
        __m256 sh01 = _mm256_insertf128_ps( _mm256_castps128_ps256( sh0 ), sh1, 1 );
        __m256 sh23 = _mm256_insertf128_ps( _mm256_castps128_ps256( sh2 ), sh3, 1 );

    #ifdef __AVX512F__
        __m512 sh0123 = _mm512_insertf32x8( _mm512_castps256_ps512( sh01 ), sh23, 1 );
        __m512 mul0 = _mm512_mask_blend_ps( 0x8888, sh0123, _mm512_set1_ps( 1.f ) );

        __m512 px = _mm512_loadu_ps( ptr );
        _mm512_storeu_ps( ptr, _mm512_mul_ps( px, mul0 ) );
    #else
        __m256 mul0 = _mm256_blend_ps( sh01, _mm256_set1_ps( 1.f ), 0x88 );
        __m256 mul1 = _mm256_blend_ps( sh23, _mm256_set1_ps( 1.f ), 0x88 );

        __m256 px0 = _mm256_loadu_ps( ptr );
        __m256 px1 = _mm256_loadu_ps( ptr + 8 );

        _mm256_storeu_ps( ptr, _mm256_mul_ps( px0, mul0 ) );
        _mm256_storeu_ps( ptr + 8, _mm256_mul_ps( px1, mul1 ) );
    #endif
  #else
        __m128 mul0 = _mm_blend_ps( sh0, _mm_set1_ps( 1.f ), 8 );
        __m128 mul1 = _mm_blend_ps( sh1, _mm_set1_ps( 1.f ), 8 );
        __m128 mul2 = _mm_blend_ps( sh2, _mm_set1_ps( 1.f ), 8 );
        __m128 mul3 = _mm_blend_ps( sh3, _mm_set1_ps( 1.f ), 8 );

        __m128 px0 = _mm_loadu_ps( ptr );
        __m128 px1 = _mm_loadu_ps( ptr + 4 );
        __m128 px2 = _mm_loadu_ps( ptr + 8 );
        __m128 px3 = _mm_loadu_ps( ptr + 12 );

        _mm_storeu_ps( ptr, _mm_mul_ps( px0, mul0 ) );
        _mm_storeu_ps( ptr + 4, _mm_mul_ps( px1, mul1 ) );
        _mm_storeu_ps( ptr + 8, _mm_mul_ps( px2, mul2 ) );
        _mm_storeu_ps( ptr + 12, _mm_mul_ps( px3, mul3 ) );
  #endif

        sz -= 4;
        ptr += 16;
        gptr += 4;
    }
#endif

    while( sz-- > 0 )
    {
        const auto gain = *gptr++;
        const auto mul = 1.f + hadj * gain;
        ptr[0] *= mul;
        ptr[1] *= mul;
        ptr[2] *= mul;
        ptr += 4;
    }
}


static void QuantizeRgba( const float* in, uint8_t* out, size_t n )
{
    while( n-- )
    {
        for( int c = 0; c < 4; c++ )
        {
            const float f = std::min( std::max( in[c], 0.f ), 1.f );
            out[c] = static_cast<uint8_t>( std::lroundf( 255.f * f ) );
        }
        in += 4;
        out += 4;
    }
}


HeifLoader::HeifLoader( std::shared_ptr<FileWrapper> file, ToneMap::Operator tonemap, TaskDispatch* td )
    : m_valid( false )
    , m_opened( false )
    , m_tonemap( tonemap )
    , m_file( std::move( file ) )
    , m_ctx( nullptr )
    , m_handle( nullptr )
    , m_handleGainMap( nullptr )
    , m_image( nullptr )
    , m_nclx( nullptr )
    , m_gainMap( nullptr )
    , m_rgb( false )
    , m_samples( 3 )
    , m_strideRGB( 0 )
    , m_planeRGB( nullptr )
    , m_iccData( nullptr )
    , m_profileIn( nullptr )
    , m_profileOut( nullptr )
    , m_transform( nullptr )
    , m_identityTransform( false )
    , m_linearizeFirst( false )
    , m_td( td )
{
    fseek( *m_file, 0, SEEK_SET );
    uint8_t hdr[12];
    if( fread( hdr, 1, 12, *m_file ) == 12 )
    {
        const auto res = heif_check_filetype( hdr, 12 );
        m_valid = res == heif_filetype_yes_supported || res == heif_filetype_maybe;
    }
}

HeifLoader::~HeifLoader()
{
    if( m_transform ) cmsDeleteTransform( m_transform );
    if( m_profileOut ) cmsCloseProfile( m_profileOut );
    if( m_profileIn ) cmsCloseProfile( m_profileIn );
    delete[] m_iccData;
    delete[] m_gainMap;
    if( m_nclx ) heif_nclx_color_profile_free( m_nclx );
    if( m_image ) heif_image_release( m_image );
    if( m_handleGainMap ) heif_image_handle_release( m_handleGainMap );
    if( m_handle ) heif_image_handle_release( m_handle );
    if( m_ctx ) heif_context_free( m_ctx );
}

bool HeifLoader::IsValidSignature( const uint8_t* buf, size_t size )
{
    if( size < 12 ) return false;
    const auto res = heif_check_filetype( buf, size );
    return res == heif_filetype_yes_supported || res == heif_filetype_maybe;
}

bool HeifLoader::IsValid() const
{
    return m_valid;
}

bool HeifLoader::IsHdr()
{
    if( !m_opened && !Open() ) return false;
    if( m_handleGainMap ) return true;
    if( m_nclx )
    {
        return
            m_nclx->transfer_characteristics == heif_transfer_characteristic_ITU_R_BT_2100_0_PQ ||
            m_nclx->transfer_characteristics == heif_transfer_characteristic_ITU_R_BT_2100_0_HLG;
    }
    return false;
}

std::unique_ptr<Bitmap> HeifLoader::Load()
{
    if( !m_opened && !Open() ) return nullptr;

    if( !IsHdr() || m_handleGainMap )
    {
        if( !SetupDecode( false, Colorspace::BT709 ) ) return nullptr;

        auto bmp = std::make_unique<Bitmap>( m_width, m_height );
        auto out = (uint32_t*)bmp->Data();

        if( m_td )
        {
            size_t offset = 0;
            size_t sz = m_width * m_height;
            while( sz > 0 )
            {
                const auto chunk = std::min( sz, size_t( 16 * 1024 ) );
                m_td->Queue( [this, out, chunk, offset] {
                    auto ptr = (float*)alloca( chunk * 4 * sizeof( float ) );
                    LoadYCbCr( ptr, chunk, offset );
                    ConvertYCbCrToRGB( ptr, chunk );
                    if( m_identityTransform ) QuantizeRgba( ptr, (uint8_t*)out, chunk );
                    else if( !m_fast || !m_fast->RunCodedToU8( nullptr, ptr, (uint8_t*)out, chunk ) ) cmsDoTransform( m_transform, ptr, out, chunk );
                } );
                out += chunk;
                sz -= chunk;
                offset += chunk;
            }
            m_td->Sync();
        }
        else
        {
            auto tmp = std::make_unique<BitmapHdr>( m_width, m_height, Colorspace::BT709 );
            LoadYCbCr( tmp->Data(), m_width * m_height, 0 );
            ConvertYCbCrToRGB( tmp->Data(), m_width * m_height );
            if( m_identityTransform ) QuantizeRgba( tmp->Data(), (uint8_t*)out, m_width * m_height );
            else if( !m_fast || !m_fast->RunCodedToU8( nullptr, tmp->Data(), (uint8_t*)out, m_width * m_height ) ) cmsDoTransform( m_transform, tmp->Data(), out, m_width * m_height );
        }

        return bmp;
    }
    else
    {
        if( m_td )
        {
            if( !SetupDecode( true, Colorspace::BT709 ) ) return nullptr;

            auto bmp = std::make_unique<Bitmap>( m_width, m_height );
            auto out = (uint32_t*)bmp->Data();

            size_t offset = 0;
            size_t sz = m_width * m_height;
            while( sz > 0 )
            {
                const auto chunk = std::min( sz, size_t( 16 * 1024 ) );
                m_td->Queue( [this, out, chunk, offset] {
                    auto ptr = (float*)alloca( chunk * 4 * sizeof( float ) );
                    LoadYCbCr( ptr, chunk, offset );
                    ConvertYCbCrToRGB( ptr, chunk );
                    TransformAndTransfer( ptr, chunk, offset );
                    ToneMap::Process( m_tonemap, out, ptr, chunk );
                } );
                out += chunk;
                sz -= chunk;
                offset += chunk;
            }
            m_td->Sync();
            return bmp;
        }
        else
        {
            std::unique_ptr<BitmapHdr> hdr = LoadHdr( Colorspace::BT709 );
            return hdr ? hdr->Tonemap( m_tonemap ) : nullptr;
        }
    }
}

std::unique_ptr<BitmapHdr> HeifLoader::LoadHdr( Colorspace colorspace )
{
    if( !m_opened && !Open() ) return nullptr;
    if( !SetupDecode( true, colorspace ) ) return nullptr;

    auto bmp = std::make_unique<BitmapHdr>( m_width, m_height, colorspace );
    if( m_td )
    {
        auto ptr = bmp->Data();
        size_t offset = 0;
        size_t sz = m_width * m_height;
        while( sz > 0 )
        {
            const auto chunk = std::min( sz, size_t( 16 * 1024 ) );
            m_td->Queue( [this, ptr, chunk, offset] {
                LoadYCbCr( ptr, chunk, offset );
                ConvertYCbCrToRGB( ptr, chunk );
                TransformAndTransfer( ptr, chunk, offset );
            } );
            ptr += chunk * 4;
            sz -= chunk;
            offset += chunk;
        }
        m_td->Sync();
    }
    else
    {
        LoadYCbCr( bmp->Data(), m_width * m_height, 0 );
        ConvertYCbCrToRGB( bmp->Data(), m_width * m_height );
        TransformAndTransfer( bmp->Data(), m_width * m_height, 0 );
    }

    return bmp;
}

bool HeifLoader::Open()
{
    CheckPanic( m_valid, "Invalid HEIF file" );
    CheckPanic( !m_opened, "Already opened" );

    m_buf = std::make_unique<FileBuffer>( m_file );

    m_ctx = heif_context_alloc();
    auto err = heif_context_read_from_memory_without_copy( m_ctx, m_buf->data(), m_buf->size(), nullptr );
    if( err.code != heif_error_Ok )
    {
        heif_context_free( m_ctx );
        m_ctx = nullptr;
        return false;
    }

    err = heif_context_get_primary_image_handle( m_ctx, &m_handle );
    if( err.code != heif_error_Ok )
    {
        heif_context_free( m_ctx );
        m_ctx = nullptr;
        return false;
    }

    err = heif_image_handle_get_nclx_color_profile( m_handle, &m_nclx );
    if( err.code == heif_error_Color_profile_does_not_exist ) mclog( LogLevel::Info, "HEIF: No handle-level nclx color profile found" );

    m_iccSize = heif_image_handle_get_raw_color_profile_size( m_handle );
    if( m_iccSize == 0 )
    {
        mclog( LogLevel::Info, "HEIF: No ICC profile found" );
    }
    else
    {
        m_iccData = new char[m_iccSize];
        err = heif_image_handle_get_raw_color_profile( m_handle, m_iccData );
        if( err.code != heif_error_Ok )
        {
            mclog( LogLevel::Warning, "HEIF: Failed to read ICC profile, continuing without" );
            delete[] m_iccData;
            m_iccData = nullptr;
            m_iccSize = 0;
        }
    }

    m_width = heif_image_handle_get_width( m_handle );
    m_height = heif_image_handle_get_height( m_handle );
    if( m_width <= 0 || m_height <= 0 )
    {
        mclog( LogLevel::Error, "HEIF: Invalid primary image dimensions %dx%d", m_width, m_height );
        heif_image_handle_release( m_handle );
        m_handle = nullptr;
        heif_context_free( m_ctx );
        m_ctx = nullptr;
        // m_opened stays false, so a retry re-reads the color profiles; release what this attempt took.
        if( m_nclx )
        {
            heif_nclx_color_profile_free( m_nclx );
            m_nclx = nullptr;
        }
        delete[] m_iccData;
        m_iccData = nullptr;
        m_iccSize = 0;
        return false;
    }

    constexpr int filter = LIBHEIF_AUX_IMAGE_FILTER_OMIT_ALPHA | LIBHEIF_AUX_IMAGE_FILTER_OMIT_DEPTH;
    const auto auxnum = heif_image_handle_get_number_of_auxiliary_images( m_handle, filter );
    if( auxnum > 0 )
    {
        std::vector<heif_item_id> aux( auxnum );
        heif_image_handle_get_list_of_auxiliary_image_IDs( m_handle, filter, aux.data(), auxnum );

        for( auto item : aux )
        {
            heif_image_handle* auxHandle = nullptr;
            auto auxErr = heif_image_handle_get_auxiliary_image_handle( m_handle, item, &auxHandle );
            if( auxErr.code != heif_error_Ok ) continue;

            const char* type = nullptr;
            auxErr = heif_image_handle_get_auxiliary_type( auxHandle, &type );
            if( auxErr.code != heif_error_Ok )
            {
                heif_image_handle_release( auxHandle );
                continue;
            }

            const auto match = type && strcmp( type, "urn:com:apple:photo:2020:aux:hdrgainmap" ) == 0;
            heif_image_handle_release_auxiliary_type( auxHandle, &type );
            if( match )
            {
                const auto bitDepth = heif_image_handle_get_luma_bits_per_pixel( auxHandle );
                const auto width = heif_image_handle_get_width( auxHandle );
                const auto height = heif_image_handle_get_height( auxHandle );
                if( width == m_width / 2 && height == m_height / 2 && bitDepth == 8 )
                {
                    if( GetGainMapHeadroom( auxHandle ) ) m_handleGainMap = auxHandle;
                    else heif_image_handle_release( auxHandle );
                    break;
                }
                mclog( LogLevel::Warning, "HEIF: Invalid gain map %dx%d, %d bpp", width, height, bitDepth );
                mclog( LogLevel::Warning, "HEIF: Expected %dx%d, 8 bpp", m_width / 2, m_height / 2 );
            }
            heif_image_handle_release( auxHandle );
        }
    }

    m_opened = true;
    return true;
}

bool HeifLoader::SetupDecode( bool hdr, Colorspace colorspace )
{
    m_identityTransform = false;
    m_fast.reset();
    if( m_transform )
    {
        cmsDeleteTransform( m_transform );
        m_transform = nullptr;
    }
    if( m_profileOut )
    {
        cmsCloseProfile( m_profileOut );
        m_profileOut = nullptr;
    }
    if( m_profileIn )
    {
        cmsCloseProfile( m_profileIn );
        m_profileIn = nullptr;
    }
    if( m_image )
    {
        heif_image_release( m_image );
        m_image = nullptr;
    }
    delete[] m_gainMap;
    m_gainMap = nullptr;

    m_rgb = m_nclx && m_nclx->matrix_coefficients == heif_matrix_coefficients_RGB_GBR;
    const auto rgbChroma = heif_image_handle_has_alpha_channel( m_handle ) ? heif_chroma_interleaved_RRGGBBAA_LE : heif_chroma_interleaved_RRGGBB_LE;
    auto err = heif_decode_image( m_handle, &m_image, m_rgb ? heif_colorspace_RGB : heif_colorspace_YCbCr,
        m_rgb ? rgbChroma : heif_chroma_444, nullptr );
    if( err.code != heif_error_Ok ) return false;

    m_colorspace = colorspace;
    m_linearizeFirst = false;

    if( !m_nclx )
    {
        auto err = heif_image_get_nclx_color_profile( m_image, &m_nclx );
        if( err.code == heif_error_Color_profile_does_not_exist ) mclog( LogLevel::Info, "HEIF: No image-level nclx color profile found" );
    }
    if( !m_rgb && m_nclx && m_nclx->matrix_coefficients == heif_matrix_coefficients_RGB_GBR )
    {
        m_rgb = true;
        heif_image_release( m_image );
        m_image = nullptr;
        err = heif_decode_image( m_handle, &m_image, heif_colorspace_RGB, rgbChroma, nullptr );
        if( err.code != heif_error_Ok ) return false;
    }

    const auto imgIccSize = heif_image_get_raw_color_profile_size( m_image );
    if( imgIccSize > 0 )
    {
        mclog( LogLevel::Info, "HEIF: Found image level ICC color profile" );
        if( m_iccData )
        {
            mclog( LogLevel::Info, "HEIF: Multiple ICC profiles found, using image level (handle level: %zu bytes, image level: %zu bytes)", m_iccSize, imgIccSize );
            delete[] m_iccData;
        }
        m_iccData = new char[imgIccSize];
        m_iccSize = imgIccSize;
        heif_image_get_raw_color_profile( m_image, m_iccData );
    }

    if( hdr && !m_nclx && !m_handleGainMap )
    {
        mclog( LogLevel::Error, "HEIF: HDR requested without nclx color profile or gain map" );
        return false;
    }

    if( m_rgb )
    {
        int strideRGB;
        m_planeRGB = heif_image_get_plane_readonly( m_image, heif_channel_interleaved, &strideRGB );
        if( !m_planeRGB ) return false;
        const auto chroma = heif_image_get_chroma_format( m_image );
        if( chroma != heif_chroma_interleaved_RRGGBB_LE && chroma != heif_chroma_interleaved_RRGGBBAA_LE )
        {
            mclog( LogLevel::Error, "HEIF: Identity-matrix decode returned chroma %d", (int)chroma );
            return false;
        }
        m_samples = chroma == heif_chroma_interleaved_RRGGBBAA_LE ? 4 : 3;
        const auto bppRGB = heif_image_get_bits_per_pixel_range( m_image, heif_channel_interleaved );
        if( bppRGB < 8 || bppRGB > 16 )
        {
            mclog( LogLevel::Error, "HEIF: Unsupported bit depth %d", bppRGB );
            return false;
        }
        if( strideRGB < m_width * m_samples * 2 )
        {
            mclog( LogLevel::Error, "HEIF: Interleaved RGB stride %d too small", strideRGB );
            return false;
        }
        m_strideRGB = strideRGB;
        m_bpp = bppRGB;
        m_bppDiv = 1.f / ( ( 1 << bppRGB ) - 1 );
        m_bppDivA = m_bppDiv;
        m_planeY = m_planeCb = m_planeCr = m_planeA = nullptr;
        mclog( LogLevel::Info, "HEIF: %d bpp identity-matrix RGB%s", bppRGB, m_samples == 4 ? "A" : "" );
    }
    else
    {
        int strideY, strideCb, strideCr, strideA = 0;
        m_planeY  = heif_image_get_plane_readonly( m_image, heif_channel_Y, &strideY );
        m_planeCb = heif_image_get_plane_readonly( m_image, heif_channel_Cb, &strideCb );
        m_planeCr = heif_image_get_plane_readonly( m_image, heif_channel_Cr, &strideCr );
        m_planeA  = heif_image_get_plane_readonly( m_image, heif_channel_Alpha, &strideA );

        if( !m_planeY || !m_planeCb || !m_planeCr ) return false;
        if( strideY != strideCb || strideY != strideCr || ( m_planeA && strideA != strideY ) )
        {
            mclog( LogLevel::Error, "HEIF: Decoded output is not 444 (strides %d/%d/%d/%d)", strideY, strideCb, strideCr, strideA );
            return false;
        }
        m_stride = strideY;

        const auto bppY = heif_image_get_bits_per_pixel_range( m_image, heif_channel_Y );
        const auto bppCb = heif_image_get_bits_per_pixel_range( m_image, heif_channel_Cb );
        const auto bppCr = heif_image_get_bits_per_pixel_range( m_image, heif_channel_Cr );
        if( bppY != bppCb || bppY != bppCr )
        {
            mclog( LogLevel::Error, "HEIF: Decoded output has mixed bit width (%d/%d/%d)", bppY, bppCb, bppCr );
            return false;
        }
        if( bppY < 8 || bppY > 16 )
        {
            mclog( LogLevel::Error, "HEIF: Unsupported bit depth %d", bppY );
            return false;
        }
        if( strideY < m_width * ( bppY > 8 ? 2 : 1 ) )
        {
            // ProcessYCbCr walks rows by ( stride - width ) in size_t; a plane narrower than the
            // handle width would wrap the row offset into a wild pointer. Strides are equal across
            // planes, so checking Y covers all of them
            mclog( LogLevel::Error, "HEIF: Y plane stride %d too small for width %d", strideY, m_width );
            return false;
        }

        m_bpp = bppY;
        m_bppDiv = 1.f / ( ( 1 << bppY ) - 1 );
        mclog( LogLevel::Info, "HEIF: %d bpp", bppY );

        m_bppDivA = m_bppDiv;
        if( m_planeA )
        {
            const auto bppA = heif_image_get_bits_per_pixel_range( m_image, heif_channel_Alpha );
            if( bppA < 8 || bppA > 16 )
            {
                mclog( LogLevel::Error, "HEIF: Unsupported alpha bit depth %d", bppA );
                return false;
            }
            m_bppDivA = 1.f / ( ( 1 << bppA ) - 1 );
        }

        if( bppY > 8 ) m_stride /= 2;
    }

    // libheif normalises decoded planes to full range and reports the result on the image-level nclx,
    // so the file-level VideoFullRangeFlag must not trigger any further scaling

    m_matrix = Conversion::BT601;
    if( m_nclx )
    {
        switch( m_nclx->matrix_coefficients )
        {
        case heif_matrix_coefficients_RGB_GBR:
            // identity matrix: decoded through the interleaved RGB path, so no conversion applies
            mclog( LogLevel::Info, "HEIF: Matrix coefficients GBR" );
            break;
        case heif_matrix_coefficients_ITU_R_BT_709_5:
            m_matrix = Conversion::BT709;
            mclog( LogLevel::Info, "HEIF: Matrix coefficients BT.709" );
            break;
        case heif_matrix_coefficients_unspecified:      // see https://github.com/AOMediaCodec/libavif/wiki/CICP
        case heif_matrix_coefficients_ITU_R_BT_470_6_System_B_G:
        case heif_matrix_coefficients_ITU_R_BT_601_6:
            m_matrix = Conversion::BT601;
            mclog( LogLevel::Info, "HEIF: Matrix coefficients BT.601" );
            break;
        case heif_matrix_coefficients_ITU_R_BT_2020_2_non_constant_luminance:
        case heif_matrix_coefficients_ITU_R_BT_2020_2_constant_luminance:
            m_matrix = Conversion::BT2020;
            mclog( LogLevel::Info, "HEIF: Matrix coefficients BT.2020" );
            break;
        default:
            mclog( LogLevel::Error, "HEIF: Matrix coefficients %d not implemented, defaulting to BT.601", m_nclx->matrix_coefficients );
            break;
        }

        switch( m_nclx->transfer_characteristics )
        {
        case heif_transfer_characteristic_ITU_R_BT_709_5:
        case heif_transfer_characteristic_unspecified:
        case heif_transfer_characteristic_IEC_61966_2_1:
            mclog( LogLevel::Debug, "HEIF: Applying SDR transfer function" );
            break;
        case heif_transfer_characteristic_ITU_R_BT_2100_0_PQ:
            mclog( LogLevel::Info, "HEIF: Applying PQ transfer function" );
            break;
        case heif_transfer_characteristic_ITU_R_BT_2100_0_HLG:
            mclog( LogLevel::Info, "HEIF: Applying HLG transfer function" );
            break;
        default:
            mclog( LogLevel::Error, "HEIF: Transfer characteristics %d not implemented", m_nclx->transfer_characteristics );
            break;
        }
    }

    cmsToneCurve* linear = cmsBuildGamma( nullptr, 1 );
    cmsToneCurve* linear3[3] = { linear, linear, linear };

    // H.273 (07/24), Table 3: the SDR input profile needs the coded transfer curve, and sRGB in the absence
    // of nclx. The HDR branches use linear TRCs (the EOTF is applied to the samples) or the ICC's own response
    cmsToneCurve* sdrCurve = nullptr;
    if( !hdr )
    {
        // PQ/HLG reach the SDR branch only as the fallback base of a gain map file, which is rendered
        // with the sRGB convention on purpose; fold the transfer here so the assumption does not log
        // as unimplemented
        int transfer = m_nclx ? m_nclx->transfer_characteristics : heif_transfer_characteristic_IEC_61966_2_1;
        if( transfer == heif_transfer_characteristic_ITU_R_BT_2100_0_PQ ||
            transfer == heif_transfer_characteristic_ITU_R_BT_2100_0_HLG )
            transfer = heif_transfer_characteristic_IEC_61966_2_1;
        sdrCurve = BuildTransferToneCurve( transfer, "HEIF" );
    }
    cmsToneCurve* sdrCurve3[3] = { sdrCurve, sdrCurve, sdrCurve };

    // A profile that fails to open or to yield a transform must not fail the load: files may carry both
    // ICC and nclx, and the nclx path below stays fully functional
    if( m_iccData )
    {
        m_profileIn = cmsOpenProfileFromMem( m_iccData, m_iccSize );
        if( !m_profileIn )
        {
            mclog( LogLevel::Warning, "HEIF: Failed to open ICC profile, continuing without" );
        }
        else
        {
            int outType;
            if( hdr )
            {
                CheckPanic( colorspace == Colorspace::BT709 || colorspace == Colorspace::BT2020, "Invalid colorspace" );
                m_profileOut = cmsCreateRGBProfile( &white709, colorspace == Colorspace::BT709 ? &primaries709 : &primaries2020, linear3 );
                outType = TYPE_RGBA_FLT;
            }
            else
            {
                m_profileOut = cmsCreate_sRGBProfile();
                outType = TYPE_RGBA_8;
            }
            m_transform = cmsCreateTransform( m_profileIn, TYPE_RGBA_FLT, m_profileOut, outType, INTENT_PERCEPTUAL, cmsFLAGS_COPY_ALPHA );
            if( !m_transform )
            {
                mclog( LogLevel::Warning, "HEIF: Failed to create ICC color transform, continuing without" );
                cmsCloseProfile( m_profileOut );
                m_profileOut = nullptr;
                cmsCloseProfile( m_profileIn );
                m_profileIn = nullptr;
            }
        }
    }

    if( !m_transform && m_nclx )
    {
        const cmsCIExyY white = { m_nclx->color_primary_white_x, m_nclx->color_primary_white_y, 1 };
        const cmsCIExyYTRIPLE primaries = {
            { m_nclx->color_primary_red_x, m_nclx->color_primary_red_y, 1 },
            { m_nclx->color_primary_green_x, m_nclx->color_primary_green_y, 1 },
            { m_nclx->color_primary_blue_x, m_nclx->color_primary_blue_y, 1 }
        };

        if( hdr )
        {
            CheckPanic( colorspace == Colorspace::BT709 || colorspace == Colorspace::BT2020, "Invalid colorspace" );
            // colour management of PQ/HLG data has to happen in linear light, so run the EOTF before the
            // primaries matrix. Gain map math runs on the transformed base and ICC profiles carry their
            // own tone response, both keep the historical order
            m_linearizeFirst = !m_handleGainMap &&
                ( m_nclx->transfer_characteristics == heif_transfer_characteristic_ITU_R_BT_2100_0_PQ ||
                  m_nclx->transfer_characteristics == heif_transfer_characteristic_ITU_R_BT_2100_0_HLG );

            const cmsCIExyYTRIPLE* outPrimaries = colorspace == Colorspace::BT709 ? &primaries709 : &primaries2020;
            if( !SameRgbPrimaries( primaries, *outPrimaries ) || !SameWhite( white, white709 ) )
            {
                m_profileIn = cmsCreateRGBProfile( &white, &primaries, linear3 );
                m_profileOut = cmsCreateRGBProfile( &white709, outPrimaries, linear3 );
                m_transform = cmsCreateTransform( m_profileIn, TYPE_RGBA_FLT, m_profileOut, TYPE_RGBA_FLT, INTENT_PERCEPTUAL, cmsFLAGS_COPY_ALPHA );
                m_fast.emplace( m_profileIn, m_profileOut );
            }
        }
        else if( ( m_nclx->transfer_characteristics == heif_transfer_characteristic_IEC_61966_2_1 ||
                   m_nclx->transfer_characteristics == heif_transfer_characteristic_unspecified ) &&
                 SameRgbPrimaries( primaries, primaries709 ) && SameWhite( white, white709 ) )
        {
            mclog( LogLevel::Info, "HEIF: SDR transform is identity, skipping color management" );
            m_identityTransform = true;
        }
        else
        {
            m_profileIn = cmsCreateRGBProfile( &white, &primaries, sdrCurve3 );
            m_profileOut = cmsCreate_sRGBProfile();
            m_transform = cmsCreateTransform( m_profileIn, TYPE_RGBA_FLT, m_profileOut, TYPE_RGBA_8, INTENT_PERCEPTUAL, cmsFLAGS_COPY_ALPHA );
            m_fast.emplace( m_profileIn, m_profileOut );
            if( !m_fast->PrepareCoded( m_bpp ) ) m_fast.reset();
        }
    }
    else if( !m_transform )
    {
        // HDR reaches this branch only for gain-map images with no usable color profile, so no color
        // management applies
        if( !hdr )
        {
            mclog( LogLevel::Info, "HEIF: SDR transform is identity, skipping color management" );
            m_identityTransform = true;
        }
    }

    cmsFreeToneCurve( linear );
    cmsFreeToneCurve( sdrCurve );

    if( !m_transform && !m_identityTransform && ( !hdr || m_profileOut ) )
    {
        mclog( LogLevel::Error, "HEIF: Failed to create color transform" );
        return false;
    }

    if( hdr && m_handleGainMap )
    {
        heif_image* gainMap;
        err = heif_decode_image( m_handleGainMap, &gainMap, heif_colorspace_monochrome, heif_chroma_monochrome, nullptr );
        if( err.code != heif_error_Ok ) return false;

        int stride;
        auto src = heif_image_get_plane_readonly( gainMap, heif_channel_Y, &stride );
        if( !src )
        {
            heif_image_release( gainMap );
            return false;
        }

        const auto w = heif_image_handle_get_width( m_handleGainMap );
        const auto h = heif_image_handle_get_height( m_handleGainMap );
        if( w != m_width / 2 || h != m_height / 2 )
        {
            mclog( LogLevel::Error, "HEIF: Invalid gain map size %dx%d for %dx%d", w, h, m_width, m_height );
            heif_image_release( gainMap );
            return false;
        }
        if( stride < w )
        {
            mclog( LogLevel::Error, "HEIF: Gain map stride %d too small for width %d", stride, w );
            heif_image_release( gainMap );
            return false;
        }

        // Apple scales the map to image size before inverting its Rec.709 gamma, so interpolation
        // runs on coded values, see "Applying Apple HDR effect to your photos"
        std::vector<float> tmp( w * h );
        auto dst = tmp.data();
        for( int y=0; y<h; y++ )
        {
            for( int x=0; x<w; x++ )
            {
                *dst++ = float( src[y*stride + x] ) / 255.f;
            }
        }
        heif_image_release( gainMap );

        m_gainMap = new float[m_width * m_height];
        stbir_resize_float_linear( tmp.data(), w, h, 0, m_gainMap, m_width, m_height, 0, STBIR_1CHANNEL );
        for( size_t i=0; i<size_t( m_width ) * m_height; i++ )
        {
            auto& v = m_gainMap[i];
            v = v < 0.081f ? v / 4.5f : std::powf( ( v + 0.099f ) / 1.099f, 1.f / 0.45f );
        }
    }

    return true;
}

template<typename T>
static inline void ProcessYCbCrAlpha( float* ptr, const T* srcY, const T* srcCb, const T* srcCr, const T* srcA, size_t sz, size_t offset, size_t width, size_t stride, float div, float divA )
{
    const auto gap = stride - width;

    const int py = offset / width;
    int px = offset % width;

    srcY += py * stride + px;
    srcCb += py * stride + px;
    srcCr += py * stride + px;
    srcA += py * stride + px;

    for(;;)
    {
        auto line = std::min( sz, size_t( width - px ) );
        sz -= line;
        while( line-- )
        {
            *ptr++ = float(*srcY++) * div;
            *ptr++ = float(*srcCb++) * div - 0.5f;
            *ptr++ = float(*srcCr++) * div - 0.5f;
            *ptr++ = float(*srcA++) * divA;
        }
        if( sz == 0 ) break;
        px = 0;

        srcY += gap;
        srcCb += gap;
        srcCr += gap;
        srcA += gap;
    }
}

template<typename T>
static inline void ProcessYCbCr( float* ptr, const T* srcY, const T* srcCb, const T* srcCr, size_t sz, size_t offset, size_t width, size_t stride, float div )
{
    const auto gap = stride - width;

    const int py = offset / width;
    int px = offset % width;

    srcY += py * stride + px;
    srcCb += py * stride + px;
    srcCr += py * stride + px;

    for(;;)
    {
        auto line = std::min( sz, size_t( width - px ) );
        sz -= line;
        while( line-- )
        {
            *ptr++ = float(*srcY++) * div;
            *ptr++ = float(*srcCb++) * div - 0.5f;
            *ptr++ = float(*srcCr++) * div - 0.5f;
            *ptr++ = 1.f;
        }
        if( sz == 0 ) break;
        px = 0;

        srcY += gap;
        srcCb += gap;
        srcCr += gap;
    }
}

void HeifLoader::LoadRGB( float* ptr, size_t sz, size_t offset )
{
    const auto* src = reinterpret_cast< const uint16_t* >( m_planeRGB );
    const size_t rowSamples = m_strideRGB / 2;
    size_t px = offset % m_width;
    src += ( offset / m_width ) * rowSamples + px * m_samples;
    for(;;)
    {
        auto line = std::min( sz, size_t( m_width ) - px );
        sz -= line;
        while( line-- )
        {
            *ptr++ = float( src[0] ) * m_bppDiv;
            *ptr++ = float( src[1] ) * m_bppDiv;
            *ptr++ = float( src[2] ) * m_bppDiv;
            *ptr++ = m_samples == 4 ? float( src[3] ) * m_bppDivA : 1.f;
            src += m_samples;
        }
        if( sz == 0 ) break;
        px = 0;
        src += rowSamples - size_t( m_width ) * m_samples;
    }
}

void HeifLoader::LoadYCbCr( float* ptr, size_t sz, size_t offset )
{
    if( m_rgb )
    {
        LoadRGB( ptr, sz, offset );
        return;
    }
    if( m_planeA )
    {
        if( m_bpp > 8 )
        {
            ProcessYCbCrAlpha( ptr, (uint16_t*)m_planeY, (uint16_t*)m_planeCb, (uint16_t*)m_planeCr, (uint16_t*)m_planeA, sz, offset, m_width, m_stride, m_bppDiv, m_bppDivA );
        }
        else
        {
            ProcessYCbCrAlpha( ptr, m_planeY, m_planeCb, m_planeCr, m_planeA, sz, offset, m_width, m_stride, m_bppDiv, m_bppDivA );
        }
    }
    else
    {
        if( m_bpp > 8 )
        {
            ProcessYCbCr( ptr, (uint16_t*)m_planeY, (uint16_t*)m_planeCb, (uint16_t*)m_planeCr, sz, offset, m_width, m_stride, m_bppDiv );
        }
        else
        {
            ProcessYCbCr( ptr, m_planeY, m_planeCb, m_planeCr, sz, offset, m_width, m_stride, m_bppDiv );
        }
    }
}

void HeifLoader::ConvertYCbCrToRGB( float* ptr, size_t sz )
{
    if( m_rgb ) return;

    float a, b, c, d;
    switch( m_matrix )
    {
    case Conversion::BT601:
        a = 1.402f;
        b = -0.344136f;
        c = -0.714136f;
        d = 1.772f;
        break;
    case Conversion::BT709:
        a = 1.5748f;
        b = -0.1873f;
        c = -0.4681f;
        d = 1.8556f;
        break;
    case Conversion::BT2020:
        a = 1.4746f;
        b = -0.16455312684366f;
        c = -0.57135312684366f;
        d = 1.8814f;
        break;
    default:
        Panic( "Invalid conversion matrix" );
        break;
    }

#ifdef __AVX512F__
    __m512 abcd16 = _mm512_set_ps( c, d, b, a, c, d, b, a, c, d, b, a, c, d, b, a );
    while( sz >= 4 )
    {
        __m512 px0 = _mm512_loadu_ps( ptr );
        __m512 px1 = _mm512_shuffle_ps( px0, px0, _MM_SHUFFLE( 2, 1, 1, 2 ) );
        __m512 mul = _mm512_mul_ps( px1, abcd16 );
        __m512 a0 = _mm512_shuffle_ps( mul, mul, _MM_SHUFFLE( 3, 3, 3, 3 ) );
        __m512 a1 = _mm512_mask_blend_ps( 0x2222, _mm512_setzero_ps(), a0 );
        __m512 add = _mm512_add_ps( mul, a1 );
        __m512 y = _mm512_shuffle_ps( px0, px0, _MM_SHUFFLE( 0, 0, 0, 0 ) );
        __m512 rgb0 = _mm512_add_ps( y, add );
        __m512 rgb = _mm512_mask_blend_ps( 0x8888, rgb0, px0 );
        _mm512_storeu_ps( ptr, rgb );

        sz -= 4;
        ptr += 16;
    }
#endif
#ifdef __AVX2__
    __m256 abcd8 = _mm256_set_ps( c, d, b, a, c, d, b, a );
    while( sz >= 2 )
    {
        __m256 px0 = _mm256_loadu_ps( ptr );
        __m256 px1 = _mm256_shuffle_ps( px0, px0, _MM_SHUFFLE( 2, 1, 1, 2 ) );
        __m256 mul = _mm256_mul_ps( px1, abcd8 );
        __m256 a0 = _mm256_shuffle_ps( mul, mul, _MM_SHUFFLE( 3, 3, 3, 3 ) );
        __m256 a1 = _mm256_blend_ps( _mm256_setzero_ps(), a0, 0x22 );
        __m256 add = _mm256_add_ps( mul, a1 );
        __m256 y = _mm256_shuffle_ps( px0, px0, _MM_SHUFFLE( 0, 0, 0, 0 ) );
        __m256 rgb0 = _mm256_add_ps( y, add );
        __m256 rgb = _mm256_blend_ps( rgb0, px0, 0x88 );
        _mm256_storeu_ps( ptr, rgb );

        sz -= 2;
        ptr += 8;
    }
#endif
#ifdef __SSE4_1__
    __m128 abcd4 = _mm_set_ps( c, d, b, a );
    while( sz > 0 )
    {
        __m128 px0 = _mm_loadu_ps( ptr );
        __m128 px1 = _mm_shuffle_ps( px0, px0, _MM_SHUFFLE( 2, 1, 1, 2 ) );
        __m128 mul = _mm_mul_ps( px1, abcd4 );
        __m128 a0 = _mm_insert_ps( _mm_setzero_ps(), mul, 0xdd );
        __m128 add = _mm_add_ps( mul, a0 );
        __m128 y = _mm_shuffle_ps( px0, px0, _MM_SHUFFLE( 0, 0, 0, 0 ) );
        __m128 rgb0 = _mm_add_ps( y, add );
        __m128 rgb = _mm_blend_ps( rgb0, px0, 8 );
        _mm_storeu_ps( ptr, rgb );

        ptr += 4;
        sz--;
    }
#else
    while( sz > 0 )
    {
        const auto Y = ptr[0];
        const auto Cb = ptr[1];
        const auto Cr = ptr[2];

        const auto r = Y + a * Cr;
        const auto g = Y + b * Cb + c * Cr;
        const auto b = Y + d * Cb;

        ptr[0] = r;
        ptr[1] = g;
        ptr[2] = b;

        ptr += 4;
        sz--;
    }
#endif
}

void HeifLoader::ApplyTransfer( float* ptr, size_t sz, size_t offset )
{
    if( m_gainMap )
    {
        ApplyGainMap( ptr, sz, m_gainMap + offset, m_gainMapHeadroom );
    }
    else
    {
        CheckPanic( m_nclx, "No nclx color profile found" );
        CheckPanic( m_colorspace == Colorspace::BT709 || m_colorspace == Colorspace::BT2020, "Invalid colorspace" );

        const auto NominalLuminanceMul = 1.0f / 203.f;
        switch( m_nclx->transfer_characteristics )
        {
        case heif_transfer_characteristic_ITU_R_BT_2100_0_PQ:
            LinearizePq( ptr, sz, NominalLuminanceMul );
            break;
        case heif_transfer_characteristic_ITU_R_BT_2100_0_HLG:
            LinearizeHlg( ptr, sz );
            break;
        default:
            break;
        }
    }
}

void HeifLoader::TransformAndTransfer( float* ptr, size_t sz, size_t offset )
{
    if( m_linearizeFirst )
    {
        ApplyTransfer( ptr, sz, offset );
        if( m_fast && m_fast->SupportsFloat() ) m_fast->RunFloat( nullptr, ptr, ptr, sz );
        else if( m_transform ) cmsDoTransform( m_transform, ptr, ptr, sz );
    }
    else
    {
        if( m_fast && m_fast->SupportsFloat() ) m_fast->RunFloat( nullptr, ptr, ptr, sz );
        else if( m_transform ) cmsDoTransform( m_transform, ptr, ptr, sz );
        ApplyTransfer( ptr, sz, offset );
    }
}

bool HeifLoader::GetGainMapHeadroom( heif_image_handle* handle )
{
    const auto metanum = heif_image_handle_get_number_of_metadata_blocks( handle, nullptr );
    std::vector<heif_item_id> meta( metanum );
    heif_image_handle_get_list_of_metadata_block_IDs( handle, nullptr, meta.data(), metanum );

    for( auto metaitem : meta )
    {
        auto metatype = heif_image_handle_get_metadata_type( handle, metaitem );
        if( strcmp( metatype, "mime" ) == 0 )
        {
            const auto exifSize = heif_image_handle_get_metadata_size( handle, metaitem );
            std::vector<uint8_t> exifData( exifSize );
            heif_image_handle_get_metadata( handle, metaitem, exifData.data() );

            pugi::xml_document doc;
            doc.load_buffer( exifData.data(), exifSize );

            const auto desc = doc.child( "x:xmpmeta" ).child( "rdf:RDF" ).child( "rdf:Description" );
            if( desc )
            {
                const auto version = desc.child( "HDRGainMap:HDRGainMapVersion" );
                if( version )
                {
                    const auto headroom = desc.child( "HDRGainMap:HDRGainMapHeadroom" );
                    if( headroom )
                    {
                        const auto value = headroom.text().as_float();
                        if( !( value > 1.f ) )
                        {
                            mclog( LogLevel::Error, "HEIF: Invalid gain map headroom %f", value );
                            return false;
                        }
                        m_gainMapHeadroom = value;
                        mclog( LogLevel::Info, "HEIF: Gain map headroom found: %f", m_gainMapHeadroom );
                        return true;
                    }
                }
            }
        }
    }

    mclog( LogLevel::Warning, "HEIF: No gain map headroom found" );
    return false;
}
