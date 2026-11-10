#include <png.h>
#include <stdexcept>
#include <string.h>
#include <vector>

#include "PngLoader.hpp"
#include "util/Bitmap.hpp"
#include "util/FileBuffer.hpp"
#include "util/FileWrapper.hpp"
#include "util/Panic.hpp"

PngLoader::PngLoader( const std::shared_ptr<FileWrapper>& file )
    : PngLoader( std::make_shared<FileBuffer>( file ) )
{
}

PngLoader::PngLoader( std::shared_ptr<DataBuffer> buf )
{
    if( buf->size() < 8 ) return;
    if( png_sig_cmp( (png_const_bytep)buf->data(), 0, 8 ) != 0 ) return;
    m_buf = std::move( buf );
    m_offset = 8;
}

bool PngLoader::IsValidSignature( const uint8_t* buf, size_t size )
{
    if( size < 8 ) return false;
    if( png_sig_cmp( (png_const_bytep)buf, 0, 8 ) != 0 ) return false;
    return true;
}

bool PngLoader::IsValid() const
{
    return m_buf != nullptr;
}

namespace
{

struct PngError : std::runtime_error
{
    explicit PngError( const char* msg ) : std::runtime_error( msg ) {}
};

void PngThrow( png_structp, png_const_charp msg )
{
    throw PngError( msg );
}

void PngWarn( png_structp, png_const_charp msg )
{
    mclog( LogLevel::Warning, "PNG warning: %s", msg );
}

struct PngReadCtx
{
    png_structp png = nullptr;
    png_infop info = nullptr;
    png_infop end = nullptr;
    ~PngReadCtx()
    {
        if( png ) png_destroy_read_struct( &png, &info, &end );
    }
};

} // namespace

std::unique_ptr<Bitmap> PngLoader::Load()
{
    CheckPanic( m_buf, "Invalid PNG file" );

    PngReadCtx ctx;
    png_uint_32 width = 0, height = 0;
    try
    {
        ctx.png = png_create_read_struct( PNG_LIBPNG_VER_STRING, nullptr, &PngThrow, &PngWarn );
        ctx.info = png_create_info_struct( ctx.png );
        ctx.end = png_create_info_struct( ctx.png );
        if( !ctx.png || !ctx.info || !ctx.end ) return nullptr;

        png_set_read_fn( ctx.png, this, []( png_structp png, png_bytep data, png_size_t length )
        {
            auto self = (PngLoader*)png_get_io_ptr( png );
            if( self->m_offset + length > self->m_buf->size() ) png_error( png, "Read error" );
            memcpy( data, self->m_buf->data() + self->m_offset, length );
            self->m_offset += length;
        } );
        png_set_sig_bytes( ctx.png, 8 );

        png_read_info( ctx.png, ctx.info );

        int bitDepth, colorType;
        png_get_IHDR( ctx.png, ctx.info, &width, &height, &bitDepth, &colorType, nullptr, nullptr, nullptr );

        if( bitDepth == 16 ) png_set_strip_16( ctx.png );

        bool trans = png_get_valid( ctx.png, ctx.info, PNG_INFO_tRNS ) != 0;
        if( trans ) png_set_tRNS_to_alpha( ctx.png );

        switch( colorType )
        {
        case PNG_COLOR_TYPE_PALETTE:
            png_set_palette_to_rgb( ctx.png );
            if( !trans ) png_set_filler( ctx.png, 0xFF, PNG_FILLER_AFTER );
            break;
        case PNG_COLOR_TYPE_GRAY:
            if( bitDepth < 8 ) png_set_expand_gray_1_2_4_to_8( ctx.png );
            png_set_gray_to_rgb( ctx.png );
            if( !trans ) png_set_filler( ctx.png, 0xFF, PNG_FILLER_AFTER );
            break;
        case PNG_COLOR_TYPE_GRAY_ALPHA:
            if( bitDepth < 8 ) png_set_expand_gray_1_2_4_to_8( ctx.png );
            png_set_gray_to_rgb( ctx.png );
            break;
        case PNG_COLOR_TYPE_RGB:
            if( !trans ) png_set_filler( ctx.png, 0xFF, PNG_FILLER_AFTER );
            break;
        default:
            break;
        }

        auto bmp = std::make_unique<Bitmap>( width, height );

        std::vector<png_bytep> rowPtrs( height );
        auto ptr = bmp->Data();
        for( png_uint_32 i=0; i<height; i++ )
        {
            rowPtrs[i] = ptr;
            ptr += width * 4;
        }
        png_read_image( ctx.png, rowPtrs.data() );

        png_read_end( ctx.png, ctx.end );
        return bmp;
    }
    catch( const PngError& e )
    {
        mclog( LogLevel::Error, "PNG decode failed: %s", e.what() );
    }
    catch( const std::bad_alloc& )
    {
        mclog( LogLevel::Error, "PNG decode failed: out of memory for %ux%u image", width, height );
    }
    return nullptr;
}
