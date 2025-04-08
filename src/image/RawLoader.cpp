#include <libraw.h>
#include <stdint.h>
#include <string.h>

#include "RawLoader.hpp"
#include "util/Bitmap.hpp"
#include "util/BitmapHdr.hpp"
#include "util/FileWrapper.hpp"
#include "util/Panic.hpp"

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

RawLoader::RawLoader( const std::shared_ptr<FileWrapper>& file )
    : m_raw( std::make_unique<LibRaw>() )
    , m_stream( std::make_unique<RawLoaderDataStream>( file ) )
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

    m_raw->unpack();
    m_raw->dcraw_process();
    auto img = m_raw->dcraw_make_mem_image();

    auto bmp = std::make_unique<Bitmap>( img->width, img->height );
    auto src = img->data;
    auto dst = (uint32_t*)bmp->Data();
    auto sz = img->width * img->height;

    switch( img->colors )
    {
    case 1:
        do
        {
            const auto v = *src++;
            *dst++ = v | (v << 8) | (v << 16) | 0xff000000;
        }
        while( --sz );
        break;
    case 3:
        {
            if( sz > 1 )
            {
                sz--;
                do
                {
                    uint32_t v;
                    memcpy( &v, src, 4 );
                    *dst++ = v | 0xff000000;
                    src += 3;
                }
                while( --sz );
            }

            const auto r = *src++;
            const auto g = *src++;
            const auto b = *src;
            *dst = r | (g << 8) | (b << 16) | 0xff000000;
        }
        break;
    default:
        break;
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

    m_raw->unpack();
    m_raw->dcraw_process();
    auto img = m_raw->dcraw_make_mem_image();

    auto bmp = std::make_unique<BitmapHdr>( img->width, img->height, colorspace );
    auto src = (uint16_t*)img->data;
    auto dst = bmp->Data();
    auto sz = img->width * img->height;

    auto color = m_raw->imgdata.color;
    const auto div = 1.f / color.maximum;

    switch( img->colors )
    {
    case 1:
        do
        {
            const auto v = *src++;
            *dst++ = v * div;
            *dst++ = v * div;
            *dst++ = v * div;
            *dst++ = 1;
        }
        while( --sz );
        break;
    case 3:
        do
        {
            *dst++ = *src++ * div;
            *dst++ = *src++ * div;
            *dst++ = *src++ * div;
            *dst++ = 1;
        }
        while( --sz );
        break;
    default:
        break;
    }

    LibRaw::dcraw_clear_mem( img );
    return bmp;
}
