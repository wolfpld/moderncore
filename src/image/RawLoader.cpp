#include <libraw.h>
#include <string.h>

#include "RawLoader.hpp"
#include "util/Bitmap.hpp"
#include "util/FileBuffer.hpp"
#include "util/Panic.hpp"

RawLoader::RawLoader( FileWrapper& file )
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

Bitmap* RawLoader::Load()
{
    CheckPanic( m_valid, "Invalid RAW file" );

    m_raw->unpack();
    m_raw->dcraw_process();
    auto img = m_raw->dcraw_make_mem_image();

    auto bmp = new Bitmap( img->width, img->height );
    auto src = img->data;
    auto dst = bmp->Data();
    auto sz = img->width * img->height;
    switch( img->colors )
    {
    case 1:
        do
        {
            const auto v = *src++;
            *dst++ = v;
            *dst++ = v;
            *dst++ = v;
            *dst++ = 255;
        }
        while( --sz );
        break;
    case 3:
        do
        {
            *dst++ = *src++;
            *dst++ = *src++;
            *dst++ = *src++;
            *dst++ = 255;
        }
        while( --sz );
        break;
    default:
        break;
    }

    m_raw->dcraw_clear_mem( img );
    return bmp;
}
