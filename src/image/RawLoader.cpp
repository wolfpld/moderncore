#include <algorithm>

#include <libraw.h>
#include <stdint.h>

#include "RawLoader.hpp"
#include "util/Bitmap.hpp"
#include "util/BitmapHdr.hpp"
#include "util/FileWrapper.hpp"
#include "util/Logs.hpp"
#include "util/Panic.hpp"
#include "util/TaskDispatch.hpp"

class RawLoaderDataStream : public LibRaw_abstract_datastream
{
public:
    RawLoaderDataStream( std::shared_ptr<FileWrapper> _file ) : file( std::move( _file ) )
    {
        fseek( *file, 0, SEEK_END );
        sz = ftell( *file );
        fseek( *file, 0, SEEK_SET );
    }

    int valid() override { return 1; }
    int read(void *ptr, size_t size, size_t nmemb) override { return fread( ptr, size, nmemb, *file ); }
    int eof() override { return feof( *file ); }
    int seek(INT64 o, int whence) override { return fseek( *file, o, whence ); }
    INT64 tell() override { return ftell( *file ); }
    INT64 size() override { return sz; }
    int get_char() override { return fgetc( *file ); }
    char *gets(char *str, int sz) override { if( sz < 1 ) return nullptr; return fgets( str, sz, *file ); }
    int scanf_one(const char *fmt, void *val) override { return fscanf( *file, fmt, val ); }
    const char *fname() override { return nullptr; }

private:
    std::shared_ptr<FileWrapper> file;
    int64_t sz;
};

static void PackMono( const uint8_t* src, uint32_t* dst, size_t count )
{
    while( count-- )
    {
        const uint32_t v = *src++;
        *dst++ = v | ( v << 8 ) | ( v << 16 ) | 0xff000000;
    }
}

static void PackRgb( const uint8_t* src, uint32_t* dst, size_t count )
{
    while( count-- )
    {
        const uint32_t r = *src++, g = *src++, b = *src++;
        *dst++ = r | ( g << 8 ) | ( b << 16 ) | 0xff000000;
    }
}

static void ExpandMono( const uint16_t* src, float* dst, size_t count )
{
    while( count-- )
    {
        float v = *src++ * ( 1.f / 65535.f );
        if( v > 0.75f )
        {
            const float t = ( v - 0.75f ) * 4.f;
            v *= 1.f + 3.f * t * t;
        }
        *dst++ = v;
        *dst++ = v;
        *dst++ = v;
        *dst++ = 1.f;
    }
}

static void ExpandRgb( const uint16_t* src, float* dst, size_t count )
{
    while( count-- )
    {
        const float r = *src++ * ( 1.f / 65535.f );
        const float g = *src++ * ( 1.f / 65535.f );
        const float b = *src++ * ( 1.f / 65535.f );
        float gain = 1.f;
        if( const float m = std::max( { r, g, b } ); m > 0.75f )
        {
            const float t = ( m - 0.75f ) * 4.f;
            gain = 1.f + 3.f * t * t;
        }
        *dst++ = r * gain;
        *dst++ = g * gain;
        *dst++ = b * gain;
        *dst++ = 1.f;
    }
}

RawLoader::RawLoader( const std::shared_ptr<FileWrapper>& file, TaskDispatch* td )
    : m_raw( std::make_unique<LibRaw>() )
    , m_stream( std::make_unique<RawLoaderDataStream>( file ) )
    , m_td( td )
{
    m_valid = m_raw->open_datastream( m_stream.get() ) == 0;
}

RawLoader::~RawLoader()
{
}

bool RawLoader::IsValid() const
{
    return m_valid;
}

std::unique_ptr<Bitmap> RawLoader::Load()
{
    CheckPanic( m_valid, "Invalid RAW file" );

    auto params = m_raw->output_params_ptr();
    params->use_camera_wb = 1;

    if( m_raw->unpack() != 0 || m_raw->dcraw_process() != 0 )
    {
        mclog( LogLevel::Warning, "RAW: failed to decode image" );
        return nullptr;
    }
    auto img = m_raw->dcraw_make_mem_image();
    if( !img || img->bits != 8 || ( img->colors != 1 && img->colors != 3 ) || !img->width || !img->height )
    {
        if( img ) LibRaw::dcraw_clear_mem( img );
        mclog( LogLevel::Warning, "RAW: failed to make image" );
        return nullptr;
    }

    auto bmp = std::make_unique<Bitmap>( img->width, img->height );
    const uint8_t* src = img->data;
    auto dst = (uint32_t*)bmp->Data();
    const auto sz = size_t( img->width ) * img->height;
    const auto convert = img->colors == 1 ? PackMono : PackRgb;

    if( m_td )
    {
        const uint8_t* s = src;
        uint32_t* d = dst;
        auto rest = sz;
        while( rest > 0 )
        {
            const auto chunk = std::min( rest, size_t( 16 * 1024 ) );
            m_td->Queue( [s, d, chunk, convert] { convert( s, d, chunk ); } );
            s += chunk * img->colors;
            d += chunk;
            rest -= chunk;
        }
        m_td->Sync();
    }
    else
    {
        convert( src, dst, sz );
    }

    LibRaw::dcraw_clear_mem( img );
    return bmp;
}

std::unique_ptr<BitmapHdr> RawLoader::LoadHdr( Colorspace colorspace )
{
    CheckPanic( m_valid, "Invalid RAW file" );

    auto params = m_raw->output_params_ptr();
    params->gamm[0] = 1;
    params->gamm[1] = 1;
    params->use_camera_wb = 1;
    params->output_color = colorspace == Colorspace::BT709 ? 1 : 8;
    params->output_bps = 16;
    params->no_auto_bright = 1;

    if( m_raw->unpack() != 0 || m_raw->dcraw_process() != 0 )
    {
        mclog( LogLevel::Warning, "RAW: failed to decode image" );
        return nullptr;
    }
    auto img = m_raw->dcraw_make_mem_image();
    if( !img || img->bits != 16 || ( img->colors != 1 && img->colors != 3 ) || !img->width || !img->height )
    {
        if( img ) LibRaw::dcraw_clear_mem( img );
        mclog( LogLevel::Warning, "RAW: failed to make image" );
        return nullptr;
    }

    auto bmp = std::make_unique<BitmapHdr>( img->width, img->height, colorspace );
    const uint16_t* src = (const uint16_t*)img->data;
    auto dst = bmp->Data();
    const auto sz = size_t( img->width ) * img->height;
    const auto convert = img->colors == 1 ? ExpandMono : ExpandRgb;

    if( m_td )
    {
        const uint16_t* s = src;
        float* d = dst;
        auto rest = sz;
        while( rest > 0 )
        {
            const auto chunk = std::min( rest, size_t( 16 * 1024 ) );
            m_td->Queue( [s, d, chunk, convert] { convert( s, d, chunk ); } );
            s += chunk * img->colors;
            d += chunk * 4;
            rest -= chunk;
        }
        m_td->Sync();
    }
    else
    {
        convert( src, dst, sz );
    }

    LibRaw::dcraw_clear_mem( img );
    return bmp;
}
