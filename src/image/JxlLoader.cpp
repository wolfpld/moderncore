#include <jxl/cms.h>
#include <jxl/cms_interface.h>
#include <jxl/color_encoding.h>
#include <jxl/decode.h>
#include <jxl/resizable_parallel_runner.h>
#include <algorithm>

#include "JxlLoader.hpp"
#include "util/Bitmap.hpp"
#include "util/BitmapHdr.hpp"
#include "util/FileBuffer.hpp"
#include "util/FileWrapper.hpp"
#include "util/Panic.hpp"
#include "util/TaskDispatch.hpp"

namespace
{
constexpr JxlColorEncoding srgb = {
    .color_space = JXL_COLOR_SPACE_RGB,
    .white_point = JXL_WHITE_POINT_D65,
    .primaries = JXL_PRIMARIES_SRGB,
    .transfer_function = JXL_TRANSFER_FUNCTION_SRGB,
    .rendering_intent = JXL_RENDERING_INTENT_PERCEPTUAL
};

constexpr JxlColorEncoding bt709 = {
    .color_space = JXL_COLOR_SPACE_RGB,
    .white_point = JXL_WHITE_POINT_D65,
    .primaries = JXL_PRIMARIES_SRGB,
    .transfer_function = JXL_TRANSFER_FUNCTION_LINEAR,
    .rendering_intent = JXL_RENDERING_INTENT_PERCEPTUAL
};

constexpr JxlColorEncoding bt2020 = {
    .color_space = JXL_COLOR_SPACE_RGB,
    .white_point = JXL_WHITE_POINT_D65,
    .primaries = JXL_PRIMARIES_2100,
    .transfer_function = JXL_TRANSFER_FUNCTION_LINEAR,
    .rendering_intent = JXL_RENDERING_INTENT_PERCEPTUAL
};
}

JxlLoader::JxlLoader( std::shared_ptr<FileWrapper> file, TaskDispatch* td )
    : m_file( std::move( file ) )
    , m_td( td )
    , m_runner( nullptr )
    , m_dec( nullptr )
{
    fseek( *m_file, 0, SEEK_SET );
    uint8_t hdr[12];
    const auto read = fread( hdr, 1, 12, *m_file );
    const auto res = JxlSignatureCheck( hdr, read );
    m_valid = res == JXL_SIG_CODESTREAM || res == JXL_SIG_CONTAINER;
}

JxlLoader::~JxlLoader()
{
    if( m_dec ) JxlDecoderDestroy( m_dec );
    if( m_runner ) JxlResizableParallelRunnerDestroy( m_runner );
}

bool JxlLoader::IsValidSignature( const uint8_t* buf, size_t size )
{
    if( size < 12 ) return false;
    const auto res = JxlSignatureCheck( buf, size );
    return res == JXL_SIG_CODESTREAM || res == JXL_SIG_CONTAINER;
}

bool JxlLoader::IsValid() const
{
    return m_valid;
}

bool JxlLoader::IsHdr()
{
    if( !m_dec && !Open() ) return false;
    return m_info.bits_per_sample > 8;
}

bool JxlLoader::PreferHdr()
{
    return true;
}

std::unique_ptr<Bitmap> JxlLoader::Load()
{
    if( !m_dec && !Open() ) return nullptr;

    auto bmp = std::make_unique<Bitmap>( m_info.xsize, m_info.ysize );

    JxlPixelFormat format = { 4, JXL_TYPE_UINT8, JXL_LITTLE_ENDIAN, 0 };
    if( JxlDecoderSetImageOutBuffer( m_dec, &format, bmp->Data(), bmp->Width() * bmp->Height() * 4 ) != JXL_DEC_SUCCESS ) return nullptr;

    for(;;)
    {
        const auto res = JxlDecoderProcessInput( m_dec );
        if( res == JXL_DEC_ERROR || res == JXL_DEC_NEED_MORE_INPUT || res == JXL_DEC_BASIC_INFO ) return nullptr;
        if( res == JXL_DEC_SUCCESS || res == JXL_DEC_FULL_IMAGE ) break;
        if( res == JXL_DEC_COLOR_ENCODING )
        {
            if( JxlDecoderSetOutputColorProfile( m_dec, &srgb, nullptr, 0 ) != JXL_DEC_SUCCESS ) return nullptr;
        }
    }

    return bmp;
}

std::unique_ptr<BitmapHdr> JxlLoader::LoadHdr( Colorspace colorspace )
{
    if( !m_dec && !Open() ) return nullptr;

    auto bmp = std::make_unique<BitmapHdr>( m_info.xsize, m_info.ysize, colorspace );

    JxlPixelFormat format = { 4, JXL_TYPE_FLOAT, JXL_LITTLE_ENDIAN, 0 };
    if( JxlDecoderSetImageOutBuffer( m_dec, &format, bmp->Data(), bmp->Width() * bmp->Height() * 4 * sizeof( float ) ) != JXL_DEC_SUCCESS ) return nullptr;

    for(;;)
    {
        const auto res = JxlDecoderProcessInput( m_dec );
        if( res == JXL_DEC_ERROR || res == JXL_DEC_NEED_MORE_INPUT || res == JXL_DEC_BASIC_INFO ) return nullptr;
        if( res == JXL_DEC_SUCCESS || res == JXL_DEC_FULL_IMAGE ) break;
        if( res == JXL_DEC_COLOR_ENCODING )
        {
            CheckPanic( colorspace == Colorspace::BT709 || colorspace == Colorspace::BT2020, "Invalid colorspace" );
            if( JxlDecoderSetOutputColorProfile( m_dec, colorspace == Colorspace::BT709 ? &bt709 : &bt2020, nullptr, 0 ) != JXL_DEC_SUCCESS ) return nullptr;
        }
    }

    // libjxl's default CMS applies intensity_target only to PQ/HLG encodings and leaves relative-luminance
    // linear output unanchored, so this pass is the sole owner of the 203 cd/m² convention
    const float floatScale = ( m_info.intensity_target > 255.f ) ? m_info.intensity_target / 203.f : 1.f;
    if( floatScale != 1.f )
    {
        const auto scale = [floatScale]( float* p, size_t pixels )
        {
            for( size_t i = pixels; i > 0; i--, p += 4 )
            {
                p[0] *= floatScale;
                p[1] *= floatScale;
                p[2] *= floatScale;
            }
        };
        auto ptr = bmp->Data();
        size_t sz = size_t( m_info.xsize ) * m_info.ysize;
        if( m_td )
        {
            while( sz > 0 )
            {
                const auto chunk = std::min( sz, size_t( 16 * 1024 ) );
                m_td->Queue( [ptr, chunk, scale]{ scale( ptr, chunk ); } );
                ptr += chunk * 4;
                sz -= chunk;
            }
            m_td->Sync();
        }
        else
        {
            scale( ptr, sz );
        }
    }

    return bmp;
}

bool JxlLoader::Open()
{
    CheckPanic( m_valid, "Invalid JPEG XL file" );
    CheckPanic( !m_buf && !m_runner && !m_dec, "Already opened" );

    m_buf = std::make_unique<FileBuffer>( m_file );
    m_runner = JxlResizableParallelRunnerCreate( nullptr );

    m_dec = JxlDecoderCreate( nullptr );
    if( JxlDecoderSubscribeEvents( m_dec, JXL_DEC_BASIC_INFO | JXL_DEC_COLOR_ENCODING | JXL_DEC_FULL_IMAGE ) != JXL_DEC_SUCCESS ||
        JxlDecoderSetParallelRunner( m_dec, JxlResizableParallelRunner, m_runner ) != JXL_DEC_SUCCESS ||
        JxlDecoderSetCms( m_dec, *JxlGetDefaultCms() ) != JXL_DEC_SUCCESS ||
        JxlDecoderSetUnpremultiplyAlpha( m_dec, JXL_TRUE ) != JXL_DEC_SUCCESS ||
        JxlDecoderSetInput( m_dec, (const uint8_t*)m_buf->data(), m_buf->size() ) != JXL_DEC_SUCCESS )
    {
        // Roll back to the pre-Open state: callers re-enter Open() while m_dec is null, and its
        // already-opened check trips on a leftover m_buf.
        JxlDecoderDestroy( m_dec );
        m_dec = nullptr;
        JxlResizableParallelRunnerDestroy( m_runner );
        m_runner = nullptr;
        m_buf.reset();
        return false;
    }

    JxlDecoderCloseInput( m_dec ); // void by contract, nothing to check

    for(;;)
    {
        const auto res = JxlDecoderProcessInput( m_dec );
        if( res == JXL_DEC_ERROR || res == JXL_DEC_NEED_MORE_INPUT || res == JXL_DEC_SUCCESS || res == JXL_DEC_FULL_IMAGE ) return false;
        if( res == JXL_DEC_BASIC_INFO )
        {
            CheckPanic( JxlDecoderGetBasicInfo( m_dec, &m_info ) == JXL_DEC_SUCCESS, "Failed to read JPEG XL basic info" );
            JxlResizableParallelRunnerSetThreads( m_runner, JxlResizableParallelRunnerSuggestThreads( m_info.xsize, m_info.ysize ) );

            return true;
        }
    }
}
