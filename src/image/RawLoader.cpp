#include <libraw.h>
#include <string.h>

#include "RawLoader.hpp"
#include "util/Bitmap.hpp"
#include "util/BitmapHdr.hpp"
#include "util/FileBuffer.hpp"
#include "util/Panic.hpp"

RawLoader::RawLoader( const std::shared_ptr<FileWrapper>& file )
    : m_raw( std::make_unique<LibRaw>() )
{
    m_buf = std::make_unique<FileBuffer>( file );
    m_valid = m_raw->open_buffer( m_buf->data(), m_buf->size() ) == 0;
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
