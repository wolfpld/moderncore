#include <png.h>
#include <stdexcept>
#include <string.h>
#include <vector>

#include "PngLoader.hpp"
#include "util/Bitmap.hpp"
#include "util/Colorspace.hpp"
#include "util/FileBuffer.hpp"
#include "util/FileWrapper.hpp"
#include "util/IccTransform.hpp"
#include "util/Panic.hpp"
#include "util/PixelBytes.hpp"
#include "TiffReader.hpp"

PngLoader::PngLoader( const std::shared_ptr<FileWrapper>& file, TaskDispatch* td )
    : PngLoader( std::make_shared<FileBuffer>( file ), td )
{
}

PngLoader::PngLoader( std::shared_ptr<DataBuffer> buf, TaskDispatch* td )
    : m_td( td )
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

template<typename F>
void WalkPngChunks( const uint8_t* buf, size_t size, F visit )
{
    size_t pos = 8;
    while( pos + 12 <= size )
    {
        uint32_t length;
        memcpy( &length, buf + pos, 4 );
        length = ntohl( length );
        if( length > size - pos - 12 ) break;  // chunk extends past the buffer
        const uint8_t* name = buf + pos + 4;
        if( !visit( name, name + 4, length ) ) break;
        if( memcmp( name, "IEND", 4 ) == 0 ) break;
        pos += 12 + length;
    }
}

int FindPngOrientation( const uint8_t* buf, size_t size )
{
    int orientation = 0;
    WalkPngChunks( buf, size, [&orientation]( const uint8_t* name, const uint8_t* data, uint32_t length )
    {
        if( memcmp( name, "eXIf", 4 ) == 0 )
        {
            const int found = FindExifOrientation( data, length );
            if( found > 0 )
            {
                orientation = found;
                return false;
            }
        }
        return true;
    } );
    return orientation;
}

cmsHPROFILE BuildCicpProfile( int primaries, int transfer )
{
    const cmsCIExyY* white = &white709;
    const cmsCIExyYTRIPLE* primariesXy = &primaries709;
    switch( primaries )
    {
    case 9:
        primariesXy = &primaries2020;
        break;
    case 11:
        primariesXy = &primariesDciP3;
        white = &whiteDciP3;
        break;
    case 12:
        primariesXy = &primariesDciP3;
        break;
    case 1:
    case 2:
        break;
    default:
        mclog( LogLevel::Error, "PNG colour primaries %d not implemented, assuming BT.709", primaries );
        break;
    }

    cmsFloat64Number srgbParams[] = { 2.4, 1.0 / 1.055, 0.055 / 1.055, 1.0 / 12.92, 0.04045 };
    cmsFloat64Number bt709Params[] = { 1.0 / 0.45, 1.0 / 1.099, 0.099 / 1.099, 1.0 / 4.5, 0.081 };
    cmsToneCurve* curve = nullptr;
    switch( transfer )
    {
    case 13:
        curve = cmsBuildParametricToneCurve( nullptr, 4, srgbParams );
        break;
    case 1:
    case 6:
    case 14:
    case 15:
        curve = cmsBuildParametricToneCurve( nullptr, 4, bt709Params );
        break;
    case 4:
        curve = cmsBuildGamma( nullptr, 2.2 );
        break;
    case 5:
        curve = cmsBuildGamma( nullptr, 2.8 );
        break;
    case 8:
        curve = cmsBuildGamma( nullptr, 1.0 );
        break;
    default:
        mclog( LogLevel::Error, "PNG transfer characteristic %d not implemented, assuming sRGB", transfer );
        curve = cmsBuildParametricToneCurve( nullptr, 4, srgbParams );
        break;
    }

    cmsToneCurve* curves[3] = { curve, curve, curve };
    auto profile = cmsCreateRGBProfile( white, primariesXy, curves );
    cmsFreeToneCurve( curve );
    return profile;
}

cmsHPROFILE BuildColourProfile( png_structp png, png_infop info, int cicpPrimaries, int cicpTransfer, int cicpRange )
{
    if( cicpTransfer >= 0 )
    {
        mclog( LogLevel::Info, "PNG cICP: primaries %d, transfer %d", cicpPrimaries, cicpTransfer );
        if( cicpRange != 1 ) mclog( LogLevel::Warning, "PNG cICP narrow-range flag ignored" );
        const bool srgbSignalled = ( cicpPrimaries == 1 || cicpPrimaries == 2 ) && cicpTransfer == 13;
        return srgbSignalled ? nullptr : BuildCicpProfile( cicpPrimaries, cicpTransfer );
    }

    png_charp iccName = nullptr;
    png_bytep iccData = nullptr;
    png_uint_32 iccSize = 0;
    int compression = 0;
    int intent = 0;
    double gamma = 0;
    double wx = 0, wy = 0, rx = 0, ry = 0, gx = 0, gy = 0, bx = 0, by = 0;
    if( png_get_iCCP( png, info, &iccName, &compression, &iccData, &iccSize ) )
    {
        mclog( LogLevel::Info, "ICC profile size: %u", iccSize );
        return cmsOpenProfileFromMem( iccData, iccSize );
    }
    if( !png_get_sRGB( png, info, &intent ) && png_get_gAMA( png, info, &gamma ) )
    {
        cmsCIExyY white = { 0.3127, 0.3290, 1.0 };
        cmsCIExyYTRIPLE primaries = { { 0.64, 0.33, 1.0 }, { 0.30, 0.60, 1.0 }, { 0.15, 0.06, 1.0 } };
        if( png_get_cHRM( png, info, &wx, &wy, &rx, &ry, &gx, &gy, &bx, &by ) )
        {
            white = { wx, wy, 1.0 };
            primaries = { { rx, ry, 1.0 }, { gx, gy, 1.0 }, { bx, by, 1.0 } };
        }
        cmsToneCurve* curve = cmsBuildGamma( nullptr, (cmsFloat64Number)1.0 / gamma );
        cmsToneCurve* curves[3] = { curve, curve, curve };
        auto profile = cmsCreateRGBProfile( &white, &primaries, curves );
        cmsFreeToneCurve( curve );
        return profile;
    }
    return nullptr;
}

struct PngReadCtx
{
    png_structp png = nullptr;
    png_infop info = nullptr;
    ~PngReadCtx()
    {
        if( png ) png_destroy_read_struct( &png, &info, nullptr );
    }
};

} // namespace

std::unique_ptr<Bitmap> PngLoader::Load()
{
    CheckPanic( m_buf, "Invalid PNG file" );
    QueryHeader();

    PngReadCtx ctx;
    png_uint_32 width = 0, height = 0;
    try
    {
        ctx.png = png_create_read_struct( PNG_LIBPNG_VER_STRING, nullptr, &PngThrow, &PngWarn );
        ctx.info = png_create_info_struct( ctx.png );
        if( !ctx.png || !ctx.info ) return nullptr;

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
            png_set_gray_to_rgb( ctx.png );
            break;
        case PNG_COLOR_TYPE_RGB:
            if( !trans ) png_set_filler( ctx.png, 0xFF, PNG_FILLER_AFTER );
            break;
        default:
            break;
        }

        const int orientation = FindPngOrientation( (const uint8_t*)m_buf->data(), m_buf->size() );
        if( orientation ) mclog( LogLevel::Info, "PNG orientation: %d", orientation );
        auto bmp = std::make_unique<Bitmap>( width, height, orientation );

        std::vector<png_bytep> rowPtrs( height );
        auto ptr = bmp->Data();
        for( png_uint_32 i=0; i<height; i++ )
        {
            rowPtrs[i] = ptr;
            ptr += (size_t)width * 4;
        }
        png_read_image( ctx.png, rowPtrs.data() );

        try
        {
            png_read_end( ctx.png, nullptr );
        }
        catch( const PngError& e )
        {
            mclog( LogLevel::Warning, "PNG trailer error: %s", e.what() );
        }

        cmsHPROFILE profileIn = BuildColourProfile( ctx.png, ctx.info, m_cicpPrimaries, m_cicpTransfer, m_cicpRange );
        if( profileIn )
        {
            auto profileOut = cmsCreate_sRGBProfile();
            auto transform = cmsCreateTransform( profileIn, TYPE_RGBA_8, profileOut, TYPE_RGBA_8, INTENT_PERCEPTUAL, 0 );
            if( transform )
            {
                CmsTransform( m_td, transform, bmp->Data(), bmp->Data(), PixelCount( bmp->Width(), bmp->Height() ) );
                cmsDeleteTransform( transform );
            }
            else
            {
                mclog( LogLevel::Warning, "PNG colour profile not transformable, left uncorrected" );
            }
            cmsCloseProfile( profileOut );
            cmsCloseProfile( profileIn );
        }

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

void PngLoader::QueryHeader()
{
    if( m_headerParsed ) return;
    m_headerParsed = true;
    if( !IsValid() ) return;
    WalkPngChunks( (const uint8_t*)m_buf->data(), m_buf->size(), [this]( const uint8_t* name, const uint8_t* data, uint32_t length )
    {
        if( memcmp( name, "cICP", 4 ) == 0 && length == 4 )
        {
            m_cicpPrimaries = data[0];
            m_cicpTransfer = data[1];
            m_cicpRange = data[3];
            return false;
        }
        return true;
    } );
}
