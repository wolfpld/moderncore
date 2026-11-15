#include <lcms2.h>
#include <math.h>
#include <png.h>
#include <stdexcept>
#include <string.h>
#include <vector>

#include "PngLoader.hpp"
#include "util/Bitmap.hpp"
#include "util/BitmapHdr.hpp"
#include "util/Colorspace.hpp"
#include "util/FileBuffer.hpp"
#include "util/FileWrapper.hpp"
#include "util/HdrEotf.hpp"
#include "util/IccTransform.hpp"
#include "util/Panic.hpp"
#include "util/PixelBytes.hpp"
#include "util/TransferCurve.hpp"
#include "TiffReader.hpp"

PngLoader::PngLoader( const std::shared_ptr<FileWrapper>& file, ToneMap::Operator tonemap, TaskDispatch* td )
    : PngLoader( std::make_shared<FileBuffer>( file ), tonemap, td )
{
}

PngLoader::PngLoader( std::shared_ptr<DataBuffer> buf, ToneMap::Operator tonemap, TaskDispatch* td )
    : m_td( td ), m_tonemap( tonemap )
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

    auto curve = BuildTransferToneCurve( transfer, "PNG" );

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

bool IccGamutIsWide( cmsHPROFILE prof )
{
    if( cmsGetColorSpace( prof ) != cmsSigRgbData ) return false;
    const cmsCIEXYZ* wtpt = (const cmsCIEXYZ*)cmsReadTag( prof, cmsSigMediaWhitePointTag );
    const cmsCIEXYZ* col[3] = {
        (const cmsCIEXYZ*)cmsReadTag( prof, cmsSigRedColorantTag ),
        (const cmsCIEXYZ*)cmsReadTag( prof, cmsSigGreenColorantTag ),
        (const cmsCIEXYZ*)cmsReadTag( prof, cmsSigBlueColorantTag )
    };
    if( !wtpt || !col[0] || !col[1] || !col[2] ) return false;

    const cmsCIEXYZ d65 = { white709.x / white709.y, 1, (1 - white709.x - white709.y) / white709.y };
    const cmsCIExyY tri[3] = { primaries709.Red, primaries709.Green, primaries709.Blue };
    float px[3], py[3];
    for( int i = 0; i < 3; i++ )
    {
        cmsCIEXYZ native;
        if( !cmsAdaptToIlluminant( &native, wtpt, &d65, col[i] ) ) return false;
        float sum = native.X + native.Y + native.Z;
        if( !(sum > 0) ) return false;
        px[i] = float( native.X / sum );
        py[i] = float( native.Y / sum );
    }

    constexpr float margin = 0.004f;
    for( int i = 0; i < 3; i++ )
    {
        const cmsCIExyY& a = tri[i];
        const cmsCIExyY& b = tri[(i + 1) % 3];
        const float ex = b.x - a.x, ey = b.y - a.y;
        const float len = sqrtf( ex * ex + ey * ey );
        for( int k = 0; k < 3; k++ )
            if( ex * (py[k] - a.y) - ey * (px[k] - a.x) < -margin * len ) return true;
    }
    return false;
}

} // namespace

void PngLoader::ReadFromBuffer( png_structp png, png_bytep data, png_size_t length )
{
    auto self = (PngLoader*)png_get_io_ptr( png );
    if( self->m_offset + length > self->m_buf->size() ) png_error( png, "Read error" );
    memcpy( data, self->m_buf->data() + self->m_offset, length );
    self->m_offset += length;
}

std::unique_ptr<Bitmap> PngLoader::Load()
{
    CheckPanic( m_buf, "Invalid PNG file" );
    QueryHeader();

    if( m_cicpTransfer == 16 || m_cicpTransfer == 18 )
    {
        auto hdr = LoadHdr( Colorspace::BT709 );
        if( !hdr ) return nullptr;
        return hdr->Tonemap( m_tonemap );
    }

    PngReadCtx ctx;
    png_uint_32 width = 0, height = 0;
    try
    {
        ctx.png = png_create_read_struct( PNG_LIBPNG_VER_STRING, nullptr, &PngThrow, &PngWarn );
        ctx.info = png_create_info_struct( ctx.png );
        if( !ctx.png || !ctx.info ) return nullptr;

        png_set_read_fn( ctx.png, this, &PngLoader::ReadFromBuffer );
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
        if( memcmp( name, "IHDR", 4 ) == 0 && length >= 13 )
        {
            m_bitDepth = data[8];
        }
        else if( memcmp( name, "cICP", 4 ) == 0 && length == 4 )
        {
            m_cicpPrimaries = data[0];
            m_cicpTransfer = data[1];
            m_cicpRange = data[3];
            return false;
        }
        return true;
    } );
}

bool PngLoader::HasWideGamutIcc()
{
    if( m_iccChecked ) return m_iccWide;
    m_iccChecked = true;
    if( !IsValid() ) return false;

    const size_t savedOffset = m_offset;
    PngReadCtx ctx;
    auto probe = [&]() -> bool
    {
        ctx.png = png_create_read_struct( PNG_LIBPNG_VER_STRING, nullptr, &PngThrow, &PngWarn );
        ctx.info = png_create_info_struct( ctx.png );
        if( !ctx.png || !ctx.info ) return false;
        if( setjmp( png_jmpbuf( ctx.png ) ) ) return false;
        png_set_read_fn( ctx.png, this, &PngLoader::ReadFromBuffer );
        png_set_sig_bytes( ctx.png, 8 );
        png_read_info( ctx.png, ctx.info );

        png_charp name;
        int compressionType;
        png_bytep profile;
        png_uint_32 size;
        if( !png_get_iCCP( ctx.png, ctx.info, &name, &compressionType, &profile, &size ) ) return false;

        cmsHPROFILE prof = cmsOpenProfileFromMem( profile, size );
        if( !prof ) return false;
        const bool wide = IccGamutIsWide( prof );
        cmsCloseProfile( prof );
        return wide;
    };

    bool wide = false;
    try
    {
        wide = probe();
    }
    catch( const PngError& e )
    {
        mclog( LogLevel::Warning, "PNG ICC inspection failed: %s", e.what() );
    }
    catch( const std::bad_alloc& )
    {
        mclog( LogLevel::Warning, "PNG ICC inspection failed: out of memory" );
    }
    m_offset = savedOffset;
    m_iccWide = wide;
    return wide;
}

bool PngLoader::IsHdr()
{
    QueryHeader();
    if( m_bitDepth == 16 || m_cicpTransfer == 16 || m_cicpTransfer == 18 ) return true;
    if( m_cicpPrimaries >= 0 ) return m_cicpPrimaries == 9 || m_cicpPrimaries == 11 || m_cicpPrimaries == 12;
    return HasWideGamutIcc();
}

std::unique_ptr<BitmapHdr> PngLoader::LoadHdr( Colorspace colorspace )
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

        png_set_read_fn( ctx.png, this, &PngLoader::ReadFromBuffer );
        png_set_sig_bytes( ctx.png, 8 );

        png_read_info( ctx.png, ctx.info );

        int bitDepth, colorType;
        png_get_IHDR( ctx.png, ctx.info, &width, &height, &bitDepth, &colorType, nullptr, nullptr, nullptr );

        const bool trans = png_get_valid( ctx.png, ctx.info, PNG_INFO_tRNS ) != 0;
        if( trans ) png_set_tRNS_to_alpha( ctx.png );

        int outDepth = bitDepth;
        switch( colorType )
        {
        case PNG_COLOR_TYPE_PALETTE:
            png_set_palette_to_rgb( ctx.png );
            outDepth = 8;
            if( !trans ) png_set_filler( ctx.png, 0xFF, PNG_FILLER_AFTER );
            break;
        case PNG_COLOR_TYPE_GRAY:
            if( bitDepth < 8 )
            {
                png_set_expand_gray_1_2_4_to_8( ctx.png );
                outDepth = 8;
            }
            png_set_gray_to_rgb( ctx.png );
            if( !trans ) png_set_filler( ctx.png, outDepth == 16 ? 0xFFFF : 0xFF, PNG_FILLER_AFTER );
            break;
        case PNG_COLOR_TYPE_GRAY_ALPHA:
            png_set_gray_to_rgb( ctx.png );
            break;
        case PNG_COLOR_TYPE_RGB:
            if( !trans ) png_set_filler( ctx.png, bitDepth == 16 ? 0xFFFF : 0xFF, PNG_FILLER_AFTER );
            break;
        default:
            break;
        }
        png_set_interlace_handling( ctx.png );
        png_read_update_info( ctx.png, ctx.info );

        const int orientation = FindPngOrientation( (const uint8_t*)m_buf->data(), m_buf->size() );
        if( orientation ) mclog( LogLevel::Info, "PNG orientation: %d", orientation );
        auto hdr = std::make_unique<BitmapHdr>( width, height, colorspace, orientation );

        const size_t rowBytes = png_get_rowbytes( ctx.png, ctx.info );
        std::vector<png_byte> rows( rowBytes * height );
        std::vector<png_bytep> rowPtrs( height );
        for( png_uint_32 y = 0; y < height; y++ )
            rowPtrs[y] = rows.data() + (size_t)y * rowBytes;
        png_read_image( ctx.png, rowPtrs.data() );

        float* dst = hdr->Data();
        const float inv = outDepth == 16 ? 1.f / 65535.f : 1.f / 255.f;
        for( png_uint_32 y = 0; y < height; y++ )
        {
            png_bytep row = rows.data() + (size_t)y * rowBytes;
            for( size_t i = 0; i < (size_t)width * 4; i++ )
            {
                const png_uint_32 sample = outDepth == 16 ? (uint32_t)( ( row[i * 2] << 8 ) | row[i * 2 + 1] ) : row[i];
                *dst++ = sample * inv;
            }
        }

        try
        {
            png_read_end( ctx.png, nullptr );
        }
        catch( const PngError& e )
        {
            mclog( LogLevel::Warning, "PNG trailer error: %s", e.what() );
        }

        cmsToneCurve* linear = cmsBuildGamma( nullptr, 1.0 );
        cmsToneCurve* linear3[3] = { linear, linear, linear };
        const cmsCIExyYTRIPLE* dstPrimaries = colorspace == Colorspace::BT2020 ? &primaries2020 : &primaries709;
        auto profileOut = cmsCreateRGBProfile( &white709, dstPrimaries, linear3 );

        cmsHPROFILE profileIn = nullptr;
        if( m_cicpTransfer == 16 )
        {
            mclog( LogLevel::Info, "PNG HDR: applying PQ transfer function" );
            LinearizePq( hdr->Data(), PixelCount( width, height ), 1.0f / 203.f );
        }
        else if( m_cicpTransfer == 18 )
        {
            mclog( LogLevel::Info, "PNG HDR: applying HLG transfer function" );
            LinearizeHlg( hdr->Data(), PixelCount( width, height ) );
        }
        else
        {
            profileIn = BuildColourProfile( ctx.png, ctx.info, m_cicpPrimaries, m_cicpTransfer, m_cicpRange );
            if( !profileIn ) profileIn = cmsCreate_sRGBProfile();
        }

        if( !profileIn )
        {
            // After the HDR EOTF the pixels are linear in whatever primaries cICP declared.
            const cmsCIExyY* srcWhite = m_cicpPrimaries == 11 ? &whiteDciP3 : &white709;
            const cmsCIExyYTRIPLE* srcPrimaries = &primaries709;
            if( m_cicpPrimaries == 9 ) srcPrimaries = &primaries2020;
            else if( m_cicpPrimaries == 11 || m_cicpPrimaries == 12 ) srcPrimaries = &primariesDciP3;
            if( srcPrimaries != dstPrimaries || srcWhite != &white709 ) profileIn = cmsCreateRGBProfile( srcWhite, srcPrimaries, linear3 );
        }
        if( profileIn )
        {
            auto transform = cmsCreateTransform( profileIn, TYPE_RGBA_FLT, profileOut, TYPE_RGBA_FLT, INTENT_PERCEPTUAL, 0 );
            if( transform )
            {
                CmsTransform( m_td, transform, hdr->Data(), hdr->Data(), PixelCount( width, height ) );
                cmsDeleteTransform( transform );
            }
            else
            {
                mclog( LogLevel::Warning, "PNG colour transform unavailable, left uncorrected" );
            }
            cmsCloseProfile( profileIn );
        }
        cmsCloseProfile( profileOut );
        cmsFreeToneCurve( linear );

        return hdr;
    }
    catch( const PngError& e )
    {
        mclog( LogLevel::Error, "PNG HDR decode failed: %s", e.what() );
    }
    catch( const std::bad_alloc& )
    {
        mclog( LogLevel::Error, "PNG HDR decode failed: out of memory for %ux%u image", width, height );
    }
    return nullptr;
}
