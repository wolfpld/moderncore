#include <algorithm>
#include <arpa/inet.h>
#include <stdio.h>
#include <jpeglib.h>
#include <lcms2.h>
#include <libexif/exif-data.h>
#include <math.h>
#include <setjmp.h>
#include <stb_image_resize2.h>
#include <stdint.h>
#include <string.h>

#include "JpgLoader.hpp"
#include "util/Colorspace.hpp"
#include "util/Bitmap.hpp"
#include "util/BitmapHdr.hpp"
#include "util/EmbedData.hpp"
#include "util/FileBuffer.hpp"
#include "util/FileWrapper.hpp"
#include "util/Panic.hpp"
#include "util/Simd.hpp"
#include "util/TaskDispatch.hpp"

#include "data/CmykIcm.hpp"

JpgLoader::JpgLoader( std::shared_ptr<FileWrapper> file, TaskDispatch* td )
    : m_file( std::move( file ) )
    , m_td( td )
    , m_cinfo( nullptr )
    , m_iccData( nullptr )
{
    fseek( *m_file, 0, SEEK_SET );
    uint8_t hdr[2];
    m_valid = fread( hdr, 1, 2, *m_file ) == 2 && hdr[0] == 0xFF && hdr[1] == 0xD8;
}

JpgLoader::~JpgLoader()
{
    free( m_iccData );
    if( m_cinfo ) jpeg_destroy_decompress( m_cinfo );
}

bool JpgLoader::IsValidSignature( const uint8_t* buf, size_t size )
{
    if( size < 2 ) return false;
    if( buf[0] != 0xFF || buf[1] != 0xD8 ) return false;
    return true;
}

bool JpgLoader::IsValid() const
{
    return m_valid;
}

bool JpgLoader::IsHdr()
{
    if( !m_cinfo && !Open() ) return false;
    return !m_grayScale && ( m_iccData || m_gainMapOffset >= 0 );
}

struct JpgErrorMgr
{
    jpeg_error_mgr pub;
    jmp_buf setjmp_buffer;
};

template<typename TInput, typename TOutput>
static void CmsTransform( TaskDispatch* td, cmsHTRANSFORM transform, const TInput* input, TOutput* output, uint32_t size )
{
    if( !td )
    {
        cmsDoTransform( transform, input, output, size );
    }
    else
    {
        while( size > 0 )
        {
            auto chunk = std::min<uint32_t>( size, 16 * 1024 );
            td->Queue( [input, output, chunk, transform] {
                cmsDoTransform( transform, input, output, chunk );
            } );
            input += chunk * 4;
            output += chunk * 4;
            size -= chunk;
        }
        td->Sync();
    }
}

namespace
{
bool HasColorspaceExtensions()
{
#ifdef JCS_EXTENSIONS
    jpeg_compress_struct cinfo;
    JpgErrorMgr jerr;

    cinfo.err = jpeg_std_error( &jerr.pub );

    if( setjmp( jerr.setjmp_buffer ) )
    {
        jpeg_destroy_compress( &cinfo );
        return false;
    }

    jpeg_create_compress( &cinfo );
    cinfo.input_components = 3;
    jpeg_set_defaults( &cinfo );
    cinfo.in_color_space = JCS_EXT_RGB;
    jpeg_default_colorspace( &cinfo );
    jpeg_destroy_compress( &cinfo );

    return true;
#else
    return false;
#endif
}
}

std::unique_ptr<Bitmap> JpgLoader::LoadNoColorspace()
{
    if( !m_cinfo && !Open() ) return nullptr;

    static const bool extensions = HasColorspaceExtensions();

    JpgErrorMgr jerr;
    m_cinfo->err = jpeg_std_error( &jerr.pub );
    jerr.pub.error_exit = []( j_common_ptr cinfo ) { longjmp( ((JpgErrorMgr*)cinfo->err)->setjmp_buffer, 1 ); };
    if( setjmp( jerr.setjmp_buffer ) ) return nullptr;

#ifdef JCS_EXTENSIONS
    if( extensions && !m_cmyk ) m_cinfo->out_color_space = JCS_EXT_RGBX;
#endif
    jpeg_start_decompress( m_cinfo );

    auto bmp = std::make_unique<Bitmap>( m_cinfo->output_width, m_cinfo->output_height, m_orientation );
    auto ptr = bmp->Data();

    if( m_cmyk || extensions )
    {
        while( m_cinfo->output_scanline < m_cinfo->output_height )
        {
            jpeg_read_scanlines( m_cinfo, &ptr, 1 );
            ptr += m_cinfo->output_width * 4;
        }
    }
    else
    {
        auto row = new uint8_t[m_cinfo->output_width * 3 + 1];
        while( m_cinfo->output_scanline < m_cinfo->output_height )
        {
            jpeg_read_scanlines( m_cinfo, &row, 1 );
            for( int i=0; i<m_cinfo->output_width; i++ )
            {
                uint32_t col;
                memcpy( &col, row + i * 3, 4 );
                col |= 0xFF000000;
                memcpy( ptr, &col, 4 );
                ptr += 4;
            }
        }
        delete[] row;
    }

    jpeg_finish_decompress( m_cinfo );
    return bmp;
}

std::unique_ptr<Bitmap> JpgLoader::Load()
{
    auto bmp = LoadNoColorspace();
    if( !bmp ) return nullptr;

    cmsHTRANSFORM transform = nullptr;
    cmsHPROFILE profileIn = nullptr;
    auto profileOut = cmsCreate_sRGBProfile();
    if( m_iccData )
    {
        mclog( LogLevel::Info, "ICC profile size: %u", m_iccSz );
        profileIn = cmsOpenProfileFromMem( m_iccData, m_iccSz );
    }
    else if( m_cmyk )
    {
        Unembed( CmykIcm );
        profileIn = cmsOpenProfileFromMem( CmykIcm->data(), CmykIcm->size() );
    }
    if( profileIn ) transform = cmsCreateTransform( profileIn, m_cmyk ? TYPE_CMYK_8_REV : TYPE_RGBA_8, profileOut, TYPE_RGBA_8, INTENT_PERCEPTUAL, 0 );
    if( transform )
    {
        CmsTransform( m_td, transform, bmp->Data(), bmp->Data(), bmp->Width() * bmp->Height() );
        cmsDeleteTransform( transform );
    }
    if( profileIn ) cmsCloseProfile( profileIn );
    cmsCloseProfile( profileOut );

    bmp->SetAlpha( 0xFF );
    return bmp;
}

namespace
{
// Bounds-checked big-endian reader for the ISO 21496-1 metadata block.
struct BeReader
{
    const uint8_t* ptr;
    size_t left;

    bool ReadU16( uint16_t& v )
    {
        if( left < 2 ) return false;
        memcpy( &v, ptr, 2 );
        v = ntohs( v );
        ptr += 2;
        left -= 2;
        return true;
    }
    bool ReadBytes( void* dst, size_t n )
    {
        if( left < n ) return false;
        memcpy( dst, ptr, n );
        ptr += n;
        left -= n;
        return true;
    }
};
}

#pragma pack( push, 1 )
struct IsoHeader
{
    uint32_t version;
    uint8_t flags;
    uint32_t baseHdrHeadroomNumerator;
    uint32_t baseHdrHeadroomDenominator;
    uint32_t alternateHdrHeadroomNumerator;
    uint32_t alternateHdrHeadroomDenominator;
};
#pragma pack( pop )

struct IsoChannel
{
    int32_t gainMapMinNumerator;
    uint32_t gainMapMinDenominator;
    int32_t gainMapMaxNumerator;
    uint32_t gainMapMaxDenominator;
    uint32_t gammaNumerator;
    uint32_t gammaDenominator;
    int32_t baseOffsetNumerator;
    uint32_t baseOffsetDenominator;
    int32_t alternateOffsetNumerator;
    uint32_t alternateOffsetDenominator;
};

struct Channel
{
    float gamma;
    float gainMapMin, gainMapMax;
    float offsetSdr, offsetHdr;
};

static bool ReadIsoChannel( BeReader& rd, Channel& out )
{
    IsoChannel ch;
    if( !rd.ReadBytes( &ch, sizeof( IsoChannel ) ) ) return false;

    ch.gainMapMinNumerator = ntohl( ch.gainMapMinNumerator );
    ch.gainMapMinDenominator = ntohl( ch.gainMapMinDenominator );
    ch.gainMapMaxNumerator = ntohl( ch.gainMapMaxNumerator );
    ch.gainMapMaxDenominator = ntohl( ch.gainMapMaxDenominator );
    ch.gammaNumerator = ntohl( ch.gammaNumerator );
    ch.gammaDenominator = ntohl( ch.gammaDenominator );
    ch.baseOffsetNumerator = ntohl( ch.baseOffsetNumerator );
    ch.baseOffsetDenominator = ntohl( ch.baseOffsetDenominator );
    ch.alternateOffsetNumerator = ntohl( ch.alternateOffsetNumerator );
    ch.alternateOffsetDenominator = ntohl( ch.alternateOffsetDenominator );

    out = {
        .gamma = float( ch.gammaNumerator ) / float( ch.gammaDenominator ),
        .gainMapMin = float( ch.gainMapMinNumerator ) / float( ch.gainMapMinDenominator ),
        .gainMapMax = float( ch.gainMapMaxNumerator ) / float( ch.gainMapMaxDenominator ),
        .offsetSdr = float( ch.baseOffsetNumerator ) / float( ch.baseOffsetDenominator ),
        .offsetHdr = float( ch.alternateOffsetNumerator ) / float( ch.alternateOffsetDenominator )
    };
    return true;
}

static void ApplyGainMap( float* dst, const float* sdr, const float* gm, size_t sz, Channel* ch )
{
#if defined __SSE4_1__ && defined __FMA__
    const float gainMapMin[4] = { ch[0].gainMapMin, ch[1].gainMapMin, ch[2].gainMapMin, 0 };
    const float gainMapMax[4] = { ch[0].gainMapMax, ch[1].gainMapMax, ch[2].gainMapMax, 0 };
    const float offsetSdr[4] = { ch[0].offsetSdr, ch[1].offsetSdr, ch[2].offsetSdr, 0 };
    const float offsetHdr[4] = { ch[0].offsetHdr, ch[1].offsetHdr, ch[2].offsetHdr, 0 };

    __m128 vGainMapMin = _mm_loadu_ps( gainMapMin );
    __m128 vGainMapMax = _mm_loadu_ps( gainMapMax );
    __m128 vOffsetSdr = _mm_loadu_ps( offsetSdr );
    __m128 vOffsetHdr = _mm_loadu_ps( offsetHdr );

#ifdef __AVX512F__
    __m512 vGainMapMin512 = _mm512_broadcast_f32x4( vGainMapMin );
    __m512 vGainMapMax512 = _mm512_broadcast_f32x4( vGainMapMax );
    __m512 vOffsetSdr512 = _mm512_broadcast_f32x4( vOffsetSdr );
    __m512 vOffsetHdr512 = _mm512_broadcast_f32x4( vOffsetHdr );

    while( sz >= 4 )
    {
        __m512 vGm = _mm512_loadu_ps( gm );
        __m512 vGm1 = _mm512_sub_ps( _mm512_set1_ps( 1.f ), vGm );
        __m512 vGm2 = _mm512_mul_ps( vGm, vGainMapMax512 );
        __m512 logBoost = _mm512_fmadd_ps( vGm1, vGainMapMin512, vGm2 );
        __m512 logBoostExp = _mm512_exp_ps( logBoost );

        __m512 vSdr = _mm512_loadu_ps( sdr );
        __m512 vSdr1 = _mm512_add_ps( vSdr, vOffsetSdr512 );
        __m512 vHdr = _mm512_fmsub_ps( vSdr1, logBoostExp, vOffsetHdr512 );
        __m512 vHdr1 = _mm512_mask_blend_ps( 0x8888, vHdr, _mm512_set1_ps( 1.f ) );

        _mm512_storeu_ps( dst, vHdr1 );

        dst += 16;
        gm += 16;
        sdr += 16;
        sz -= 4;
    }
#endif

#ifdef __AVX2__
    __m256 vGainMapMin256 = _mm256_set_m128( vGainMapMin, vGainMapMin );
    __m256 vGainMapMax256 = _mm256_set_m128( vGainMapMax, vGainMapMax );
    __m256 vOffsetSdr256 = _mm256_set_m128( vOffsetSdr, vOffsetSdr );
    __m256 vOffsetHdr256 = _mm256_set_m128( vOffsetHdr, vOffsetHdr );

    while( sz >= 2 )
    {
        __m256 vGm = _mm256_loadu_ps( gm );
        __m256 vGm1 = _mm256_sub_ps( _mm256_set1_ps( 1.f ), vGm );
        __m256 vGm2 = _mm256_mul_ps( vGm, vGainMapMax256 );
        __m256 logBoost = _mm256_fmadd_ps( vGm1, vGainMapMin256, vGm2 );
        __m256 logBoostExp = _mm256_exp_ps( logBoost );

        __m256 vSdr = _mm256_loadu_ps( sdr );
        __m256 vSdr1 = _mm256_add_ps( vSdr, vOffsetSdr256 );
        __m256 vHdr = _mm256_fmsub_ps( vSdr1, logBoostExp, vOffsetHdr256 );
        __m256 vHdr1 = _mm256_blend_ps( vHdr, _mm256_set1_ps( 1.f ), 0x88 );

        _mm256_storeu_ps( dst, vHdr1 );

        gm += 8;
        sdr += 8;
        dst += 8;
        sz -= 2;
    }
#endif

    while( sz-- > 0 )
    {
        __m128 vGm = _mm_loadu_ps( gm );
        __m128 vGm1 = _mm_sub_ps( _mm_set1_ps( 1.f ), vGm );
        __m128 vGm2 = _mm_mul_ps( vGm, vGainMapMax );
        __m128 logBoost = _mm_fmadd_ps( vGm1, vGainMapMin, vGm2 );
        __m128 logBoostExp = _mm_exp_ps( logBoost );

        __m128 vSdr = _mm_loadu_ps( sdr );
        __m128 vSdr1 = _mm_add_ps( vSdr, vOffsetSdr );
        __m128 vHdr = _mm_fmsub_ps( vSdr1, logBoostExp, vOffsetHdr );
        __m128 vHdr1 = _mm_blend_ps( vHdr, _mm_set1_ps( 1.f ), 0x8 );

        _mm_storeu_ps( dst, vHdr1 );

        gm += 4;
        sdr += 4;
        dst += 4;
    }
#else
    while( sz-- > 0 )
    {
        const auto gmR = *gm++;
        const auto gmG = *gm++;
        const auto gmB = *gm++;
        gm++;

        const auto logBoostR = ch[0].gainMapMin * ( 1.f - gmR ) + ch[0].gainMapMax * gmR;
        const auto logBoostG = ch[1].gainMapMin * ( 1.f - gmG ) + ch[1].gainMapMax * gmG;
        const auto logBoostB = ch[2].gainMapMin * ( 1.f - gmB ) + ch[2].gainMapMax * gmB;

        const auto r = ( *sdr++ + ch[0].offsetSdr ) * exp2( logBoostR ) - ch[0].offsetHdr;
        const auto g = ( *sdr++ + ch[1].offsetSdr ) * exp2( logBoostG ) - ch[1].offsetHdr;
        const auto b = ( *sdr++ + ch[2].offsetSdr ) * exp2( logBoostB ) - ch[2].offsetHdr;
        sdr++;

        *dst++ = r;
        *dst++ = g;
        *dst++ = b;
        *dst++ = 1.0f;
    }
#endif
}

std::unique_ptr<BitmapHdr> JpgLoader::LoadHdr( Colorspace colorspace )
{
    if( !IsHdr() ) return nullptr;

    auto base = LoadNoColorspace();
    if( !base ) return nullptr;

    auto baseFloat = std::make_unique<BitmapHdr>( base->Width(), base->Height(), colorspace, m_orientation );

    cmsToneCurve* linear = cmsBuildGamma( nullptr, 1 );
    cmsToneCurve* linear3[3] = { linear, linear, linear };
    cmsHPROFILE profileIn;
    auto profileOut = cmsCreateRGBProfile( &white709, colorspace == Colorspace::BT2020 ? &primaries2020 : &primaries709, linear3 );
    if( m_iccData )
    {
        mclog( LogLevel::Info, "ICC profile size: %u", m_iccSz );
        profileIn = cmsOpenProfileFromMem( m_iccData, m_iccSz );
    }
    else if( m_cmyk )
    {
        Unembed( CmykIcm );
        profileIn = cmsOpenProfileFromMem( CmykIcm->data(), CmykIcm->size() );
    }
    else
    {
        profileIn = cmsCreate_sRGBProfile();
    }
    auto transform = cmsCreateTransform( profileIn, m_cmyk ? TYPE_CMYK_8_REV : TYPE_RGBA_8, profileOut, TYPE_RGBA_FLT, INTENT_PERCEPTUAL, 0 );
    if( transform )
    {
        CmsTransform( m_td, transform, base->Data(), baseFloat->Data(), base->Width() * base->Height() );
    }
    else
    {
        auto sz = base->Width() * base->Height();
        cmsHPROFILE profileSrgb = cmsCreate_sRGBProfile();
        transform = cmsCreateTransform( profileIn, m_cmyk ? TYPE_CMYK_8_REV : TYPE_RGBA_8, profileSrgb, TYPE_RGBA_8, INTENT_PERCEPTUAL, 0 );
        if( !transform )
        {
            cmsCloseProfile( profileIn );
            cmsCloseProfile( profileOut );
            cmsFreeToneCurve( linear );
            mclog( LogLevel::Error, "JPEG: Failed to create transform to sRGB" );
            return nullptr;
        }
        CmsTransform( m_td, transform, base->Data(), base->Data(), sz );
        cmsDeleteTransform( transform );
        transform = cmsCreateTransform( profileSrgb, TYPE_RGBA_8, profileOut, TYPE_RGBA_FLT, INTENT_PERCEPTUAL, 0 );
        CheckPanic( transform, "Failed to create sRGB to HDR transform" );
        CmsTransform( m_td, transform, base->Data(), baseFloat->Data(), sz );
        cmsCloseProfile( profileSrgb );
    }
    if( transform ) cmsDeleteTransform( transform );
    cmsCloseProfile( profileIn );
    cmsCloseProfile( profileOut );
    cmsFreeToneCurve( linear );

    base.reset();

    if( m_gainMapOffset < 0 )
    {
        auto sz = baseFloat->Width() * baseFloat->Height();
        auto ptr = baseFloat->Data();
        while( sz-- > 0 )
        {
            // ITU-R BT.2408 reference white is 203 nits
            *ptr++ *= 2.03f;
            *ptr++ *= 2.03f;
            *ptr++ *= 2.03f;
            *ptr++ = 1.f;
        }
        return baseFloat;
    }

    fseek( *m_file, m_gainMapOffset, SEEK_SET );

    uint8_t* gainMap = nullptr;

    jpeg_decompress_struct gcinfo;
    JpgErrorMgr jerr;
    gcinfo.err = jpeg_std_error( &jerr.pub );
    jerr.pub.error_exit = []( j_common_ptr cinfo ) { longjmp( ((JpgErrorMgr*)cinfo->err)->setjmp_buffer, 1 ); };
    if( setjmp( jerr.setjmp_buffer ) )
    {
        jpeg_destroy_decompress( &gcinfo );
        delete[] gainMap;
        return nullptr;
    }

    jpeg_create_decompress( &gcinfo );
    jpeg_stdio_src( &gcinfo, *m_file );
    jpeg_save_markers( &gcinfo, JPEG_APP0 + 1, 0xFFFF );
    jpeg_save_markers( &gcinfo, JPEG_APP0 + 2, 0xFFFF );
    jpeg_read_header( &gcinfo, TRUE );

    float hdrCapMin, hdrCapMax;
    Channel ch[3];

    bool loaded = false;
    if( m_isIso )
    {
        auto marker = gcinfo.marker_list;
        while( marker )
        {
            if( marker->marker == JPEG_APP0 + 2 && marker->data_length > 28 &&
                memcmp( marker->data, "urn:iso:std:iso:ts:21496:-1\0", 28 ) == 0 )
            {
                // C.2.2 block: 4 B version (minimum, writer), flags, base/alternate headroom ratios,
                // then 40 B per channel. Trailing padding is allowed. Flag bits 0x04/0x08 are
                // reserved. C.2.3 sanctions ignoring the gain map when reserved bits are set.
                BeReader rd{ marker->data + 28, (size_t)marker->data_length - 28 };
                IsoHeader hdr;
                if( rd.ReadBytes( &hdr, sizeof( hdr ) ) && !( ntohl( hdr.version ) >> 16 ) && !( hdr.flags & 0x3F ) )
                {
                    if( ReadIsoChannel( rd, ch[0] ) )
                    {
                        if( hdr.flags & 0x80 )  // is_multichannel
                        {
                            if( ReadIsoChannel( rd, ch[1] ) && ReadIsoChannel( rd, ch[2] ) ) loaded = true;
                        }
                        else
                        {
                            ch[1] = ch[2] = ch[0];
                            loaded = true;
                        }
                    }
                }
                break;
            }
            marker = marker->next;
        }
    }
    else
    {
        auto doc = LoadXmp( &gcinfo );
        if( !doc )
        {
            mclog( LogLevel::Warning, "JPEG: No XMP metadata found for gain map" );
            jpeg_destroy_decompress( &gcinfo );
            return nullptr;
        }

        auto root = doc->child( "x:xmpmeta" ).child( "rdf:RDF" ).child( "rdf:Description" );
        if( !root )
        {
            mclog( LogLevel::Warning, "JPEG: No gain map metadata found in XMP" );
            jpeg_destroy_decompress( &gcinfo );
            return nullptr;
        }

        const auto attrGamma = root.attribute( "hdrgm:Gamma" );
        const auto attrHdrCapMin = root.attribute( "hdrgm:HDRCapacityMin" );
        const auto attrHdrCapMax = root.attribute( "hdrgm:HDRCapacityMax" );
        const auto attrGainMapMin = root.attribute( "hdrgm:GainMapMin" );
        const auto attrGainMapMax = root.attribute( "hdrgm:GainMapMax" );
        const auto attrOffsetSdr = root.attribute( "hdrgm:OffsetSDR" );
        const auto attrOffsetHdr = root.attribute( "hdrgm:OffsetHDR" );

        ch[0] = {
            .gamma = attrGamma ? attrGamma.as_float() : 1.0f,
            .gainMapMin = attrGainMapMin ? attrGainMapMin.as_float() : 0.0f,
            .gainMapMax = attrGainMapMax ? attrGainMapMax.as_float() : 1.0f,  // required value
            .offsetSdr = attrOffsetSdr ? attrOffsetSdr.as_float() : (1.f/64),
            .offsetHdr = attrOffsetHdr ? attrOffsetHdr.as_float() : (1.f/64)
        };
        ch[2] = ch[1] = ch[0];

        hdrCapMin = attrHdrCapMin ? attrHdrCapMin.as_float() : 0.0f;
        hdrCapMax = attrHdrCapMax ? attrHdrCapMax.as_float() : 1.0f;     // required value

        loaded = true;
    }

    if( !loaded )
    {
        mclog( LogLevel::Warning, "JPEG: No gain map metadata found" );
        jpeg_destroy_decompress( &gcinfo );
        return nullptr;
    }

    jpeg_start_decompress( &gcinfo );
    const auto gmChannels = gcinfo.output_components;
    gainMap = new uint8_t[gcinfo.output_width * gcinfo.output_height * gmChannels];
    auto ptr = gainMap;
    while( gcinfo.output_scanline < gcinfo.output_height )
    {
        jpeg_read_scanlines( &gcinfo, &ptr, 1 );
        ptr += gcinfo.output_width * gmChannels;
    }
    jpeg_finish_decompress( &gcinfo );

    auto gmFloat = std::make_unique<BitmapHdr>( gcinfo.output_width, gcinfo.output_height, colorspace );
    jpeg_destroy_decompress( &gcinfo );

    auto src = gainMap;
    auto dst = gmFloat->Data();
    auto sz = gmFloat->Width() * gmFloat->Height();
    if( gmChannels == 1 )
    {
        if( ch[0].gamma == 1.f )
        {
            while( sz-- > 0 )
            {
                const auto v = *src++ / 255.f;
                *dst++ = v;
                *dst++ = v;
                *dst++ = v;
                *dst++ = 1.f;
            }
        }
        else
        {
            const auto revGamma = 1.0f / ch[0].gamma;
            while( sz-- > 0 )
            {
                const auto v = powf( *src++ / 255.f, revGamma );
                *dst++ = v;
                *dst++ = v;
                *dst++ = v;
                *dst++ = 1.f;
            }
        }
    }
    else
    {
        CheckPanic( gmChannels == 3, "Unsupported JPEG gain map format" );
        if( ch[0].gamma == 1.f &&
            ch[1].gamma == 1.f &&
            ch[2].gamma == 1.f )
        {
            while( sz-- > 0 )
            {
                *dst++ = *src++ / 255.f;
                *dst++ = *src++ / 255.f;
                *dst++ = *src++ / 255.f;
                *dst++ = 1.f;
            }
        }
        else
        {
            const auto revGammaR = 1.0f / ch[0].gamma;
            const auto revGammaG = 1.0f / ch[1].gamma;
            const auto revGammaB = 1.0f / ch[2].gamma;
            while( sz-- > 0 )
            {
                *dst++ = powf( *src++ / 255.f, revGammaR );
                *dst++ = powf( *src++ / 255.f, revGammaG );
                *dst++ = powf( *src++ / 255.f, revGammaB );
                *dst++ = 1.f;
            }
        }
    }
    delete[] gainMap;

    if( gmFloat->Width() != baseFloat->Width() || gmFloat->Height() != baseFloat->Height() )
    {
        gmFloat->Resize( baseFloat->Width(), baseFloat->Height(), m_td );
    }

    auto bmp = std::make_unique<BitmapHdr>( baseFloat->Width(), baseFloat->Height(), colorspace, m_orientation );
    sz = baseFloat->Width() * baseFloat->Height();

    dst = bmp->Data();
    auto sdr = baseFloat->Data();
    auto gm = gmFloat->Data();

    ApplyGainMap( dst, sdr, gm, sz, ch );

    return bmp;
}

namespace
{
// Bounds-checked reader for the MPF TIFF block. All TIFF/IFD offsets are block-relative.
struct TiffReader
{
    const uint8_t* base;
    size_t size;
    size_t pos = 0;
    bool bigEndian;

    bool Seek( size_t to )
    {
        if( to > size ) return false;
        pos = to;
        return true;
    }
    bool ReadU16( uint16_t& v )
    {
        if( size - pos < 2 ) return false;
        memcpy( &v, base + pos, 2 );
        if( bigEndian ) v = ntohs( v );
        pos += 2;
        return true;
    }
    bool ReadU32( uint32_t& v )
    {
        if( size - pos < 4 ) return false;
        memcpy( &v, base + pos, 4 );
        if( bigEndian ) v = ntohl( v );
        pos += 4;
        return true;
    }
    void Skip( size_t n ) { pos = size - pos < n ? size : pos + n; }
};
}

// Returns the file position of the MPF TIFF header (first byte after the 'MPF\0' fourcc), or -1.
static int FindMpfTiffHeader( FILE* file )
{
    if( fseek( file, 2, SEEK_SET ) ) return -1;  // skip Start-Of-Image
    for( int steps = 4096; steps > 0; steps-- )  // positions advance monotonically. The cap is only defense in depth
    {
        int c;
        do { c = fgetc( file ); } while( c == 0xFF );
        if( c == EOF || c == 0x00 ) return -1;
        const int marker = c;
        if( marker == 0xDA || marker == 0xD9 ) return -1;  // SOS, EOI
        if( ( marker >= 0xD0 && marker <= 0xD9 ) || marker == 0x01 ) continue;
        uint8_t lenBuf[2];
        if( fread( lenBuf, 1, 2, file ) != 2 ) return -1;
        const long len = ( lenBuf[0] << 8 ) | lenBuf[1];
        if( len < 2 ) return -1;
        const long payload = ftell( file );
        if( marker == 0xE2 && len >= 6 )
        {
            uint32_t fourcc;
            if( fread( &fourcc, 1, 4, file ) == 4 && memcmp( &fourcc, "MPF\0", 4 ) == 0 ) return (int)ftell( file );
        }
        if( fseek( file, payload + len - 2, SEEK_SET ) ) return -1;
    }
    return -1;
}

bool JpgLoader::Open()
{
    CheckPanic( m_valid, "Invalid JPEG file" );
    CheckPanic( !m_cinfo, "Already opened" );

    fseek( *m_file, 0, SEEK_SET );
    m_orientation = LoadOrientation();

    m_cinfo = new jpeg_decompress_struct();

    JpgErrorMgr jerr;
    m_cinfo->err = jpeg_std_error( &jerr.pub );
    jerr.pub.error_exit = []( j_common_ptr cinfo ) { longjmp( ((JpgErrorMgr*)cinfo->err)->setjmp_buffer, 1 ); };
    if( setjmp( jerr.setjmp_buffer ) ) return false;

    jpeg_create_decompress( m_cinfo );
    jpeg_stdio_src( m_cinfo, *m_file );
    jpeg_save_markers( m_cinfo, JPEG_APP0 + 1, 0xFFFF );
    jpeg_save_markers( m_cinfo, JPEG_APP0 + 2, 0xFFFF );
    jpeg_read_header( m_cinfo, TRUE );

    jpeg_read_icc_profile( m_cinfo, &m_iccData, &m_iccSz );
    m_cmyk = m_cinfo->jpeg_color_space == JCS_CMYK || m_cinfo->jpeg_color_space == JCS_YCCK;
    m_grayScale = m_cinfo->jpeg_color_space == JCS_GRAYSCALE;

    m_gainMapOffset = -1;
    m_isIso = false;

    bool validGainMap = false;
    auto marker = m_cinfo->marker_list;
    while( marker )
    {
        if( marker->marker == JPEG_APP0 + 2 && marker->data_length > 28 &&
            memcmp( marker->data, "urn:iso:std:iso:ts:21496:-1\0", 28 ) == 0 )
        {
            BeReader rd{ marker->data + 28, (size_t)marker->data_length - 28 };
            uint16_t version;
            if( rd.ReadU16( version ) && version == 0 )  // minimum_version
            {
                validGainMap = true;
                m_isIso = true;
            }
            break;
        }
        marker = marker->next;
    }
    if( !m_isIso )
    {
        auto doc = LoadXmp( m_cinfo );
        if( doc )
        {
            if( auto root = doc->child( "x:xmpmeta" ).child( "rdf:RDF" ).child( "rdf:Description" ); root )
            {
                const auto hdrgmns = root.attribute( "xmlns:hdrgm" );
                const auto hdrgm = root.attribute( "hdrgm:Version" );
                const auto containerns = root.attribute( "xmlns:Container" );
                if( hdrgmns && strcmp( hdrgmns.as_string(), "http://ns.adobe.com/hdr-gain-map/1.0/" ) == 0 &&
                    containerns && strcmp( containerns.as_string(), "http://ns.google.com/photos/1.0/container/" ) == 0 &&
                    hdrgm && strcmp( hdrgm.as_string(), "1.0" ) == 0 )
                {
                    const auto container = root.child( "Container:Directory" ).child( "rdf:Seq" ).children( "rdf:li" );
                    for( auto li : container )
                    {
                        if( auto item = li.child( "Container:Item" ); item )
                        {
                            if( auto semantic = item.attribute( "Item:Semantic" ); semantic )
                            {
                                if( strcmp( semantic.as_string(), "GainMap" ) == 0 )
                                {
                                    validGainMap = true;
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if( validGainMap )
    {
        auto marker = m_cinfo->marker_list;
        while( marker )
        {
            if( marker->marker == JPEG_APP0 + 2 && marker->data_length > 8 && memcmp( marker->data, "MPF\0", 4 ) == 0 )
            {
                TiffReader rd{ .base = marker->data + 4, .size = (size_t)marker->data_length - 4, .bigEndian = marker->data[4] == 0x4D };
                rd.pos = 2;  // skip endian indication
                uint16_t magic = 0, count = 0;
                uint32_t ifd0 = 0;
                if( rd.ReadU16( magic ) && magic == 42 && rd.ReadU32( ifd0 ) &&
                    rd.Seek( ifd0 ) && rd.ReadU16( count ) )
                {
                    int numImages = 0;
                    for( int i = 0; i < count; i++ )
                    {
                        uint16_t tag = 0, type = 0;
                        uint32_t cnt = 0, value = 0;
                        if( !rd.ReadU16( tag ) || !rd.ReadU16( type ) || !rd.ReadU32( cnt ) || !rd.ReadU32( value ) ) break;

                        if( tag == 0xB001 )
                        {
                            numImages = value;
                        }
                        else if( tag == 0xB002 )
                        {
                            if( !rd.Seek( value ) || rd.size - rd.pos < (size_t)std::max( numImages, 0 ) * 16 ) break;
                            for( int j = 0; j < numImages; j++ )
                            {
                                uint32_t attr = 0, size = 0, offset = 0;
                                if( !rd.ReadU32( attr ) || !rd.ReadU32( size ) || !rd.ReadU32( offset ) ) break;
                                rd.Skip( 4 );  // dependent entries

                                // Undefined or GainMap
                                if( attr == 0 || ( attr & 0xFFFFFF ) == 0x050000 )
                                {
                                    m_gainMapOffset = (int)offset;
                                    break;
                                }
                            }
                            break;
                        }
                    }
                }
                break;
            }
            marker = marker->next;
        }
    }

    // Gain map offset is relative to the MPF TIFF header, whose location is not reported by libjpeg.
    if( m_gainMapOffset >= 0 )
    {
        const auto filePos = ftell( *m_file );
        const int tiffPos = FindMpfTiffHeader( *m_file );
        if( tiffPos < 0 || (int64_t)m_gainMapOffset + tiffPos > INT32_MAX ) m_gainMapOffset = -1;
        else m_gainMapOffset += tiffPos;
        if( m_gainMapOffset >= 0 )  // the resolved entry must point at the secondary image's SOI marker
        {
            uint8_t soi[2] = {};
            if( fseek( *m_file, m_gainMapOffset, SEEK_SET ) ||
                fread( soi, 1, 2, *m_file ) != 2 || soi[0] != 0xFF || soi[1] != 0xD8 )
                m_gainMapOffset = -1;
        }
        if( m_gainMapOffset < 0 ) mclog( LogLevel::Warning, "JPEG: invalid gain map entry" );
        else mclog( LogLevel::Info, "Gain map offset: %d", m_gainMapOffset );
        fseek( *m_file, filePos, SEEK_SET );
    }

    return true;
}

int JpgLoader::LoadOrientation()
{
    int orientation = 0;

    FileBuffer buf( m_file );
    auto exif = exif_data_new_from_data( (const unsigned char*)buf.data(), buf.size() );

    if( exif )
    {
        ExifEntry* entry = exif_content_get_entry( exif->ifd[EXIF_IFD_0], EXIF_TAG_ORIENTATION );
        if( entry )
        {
            orientation = exif_get_short( entry->data, exif_data_get_byte_order( exif ) );
            mclog( LogLevel::Info, "JPEG orientation: %d", orientation );
        }
        exif_data_free( exif );
    }

    return orientation;
}

std::unique_ptr<pugi::xml_document> JpgLoader::LoadXmp( jpeg_decompress_struct* cinfo )
{
    constexpr const char XmpSig[] = "http://ns.adobe.com/xap/1.0/";
    constexpr size_t XmpSigLen = sizeof( XmpSig );

    auto marker = cinfo->marker_list;
    while( marker )
    {
        if( marker->marker == JPEG_APP0 + 1 )
        {
            if( marker->data_length > XmpSigLen && memcmp( marker->data, XmpSig, XmpSigLen ) == 0 )
            {
                auto doc = std::make_unique<pugi::xml_document>();
                if( doc->load_buffer( marker->data + XmpSigLen, marker->data_length - XmpSigLen ) ) return doc;
                break;
            }
        }
        marker = marker->next;
    }

    return nullptr;
}
