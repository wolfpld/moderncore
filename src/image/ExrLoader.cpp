#include <vector>

#include <ImfChromaticities.h>
#include <ImfRgbaFile.h>
#include <ImfStandardAttributes.h>
#include <lcms2.h>

#include "ExrLoader.hpp"
#include "util/Bitmap.hpp"
#include "util/BitmapHdr.hpp"
#include "util/Colorspace.hpp"
#include "util/FileWrapper.hpp"
#include "util/Panic.hpp"
#include "util/TaskDispatch.hpp"
#include "util/Tonemapper.hpp"

static void FixAlpha( float* ptr, size_t sz )
{
    ptr += 3;
    do
    {
        *ptr = 1;
        ptr += 4;
    }
    while( --sz );
}

class ExrStream : public Imf::IStream
{
public:
    explicit ExrStream( std::shared_ptr<FileWrapper>&& file )
        : Imf::IStream( "<unknown>" )
        , m_file( std::move( file ) )
    {
        fseek( *m_file, 0, SEEK_SET );
    }

    bool isMemoryMapped() const override { return false; }
    bool read( char c[], int n ) override { fread( c, 1, n, *m_file ); return !feof( *m_file ); }
    uint64_t tellg() override { return ftell( *m_file ); }
    void seekg( uint64_t pos ) override { fseek( *m_file, pos, SEEK_SET ); }

private:
    std::shared_ptr<FileWrapper> m_file;
};

struct ExrThreadSetter
{
    ExrThreadSetter()
    {
        Imf::setGlobalThreadCount( std::max( 1u, std::thread::hardware_concurrency() ) );
    }
};

ExrLoader::ExrLoader( std::shared_ptr<FileWrapper> file, ToneMap::Operator tonemap, TaskDispatch* td )
    : m_td( td )
    , m_tonemap( tonemap )
{
    static ExrThreadSetter setter;

    try
    {
        m_stream = std::make_unique<ExrStream>( std::move( file ) );
        m_exr = std::make_unique<Imf::RgbaInputFile>( *m_stream );
        m_valid = true;
    }
    catch( const std::exception& )
    {
        m_valid = false;
    }
}

ExrLoader::~ExrLoader()
{
}

bool ExrLoader::IsValidSignature( const uint8_t* buf, size_t size )
{
    if( size < 4 ) return false;
    return memcmp( buf, "\x76\x2f\x31\x01", 4 ) == 0;
}

bool ExrLoader::IsValid() const
{
    return m_valid;
}

std::unique_ptr<Bitmap> ExrLoader::Load()
{
    auto hdr = LoadHdr( Colorspace::BT709 );
    if( m_td )
    {
        auto bmp = std::make_unique<Bitmap>( hdr->Width(), hdr->Height() );
        auto src = hdr->Data();
        auto dst = bmp->Data();
        size_t sz = hdr->Width() * hdr->Height();
        while( sz > 0 )
        {
            const auto chunk = std::min( sz, size_t( 16 * 1024 ) );
            m_td->Queue( [src, dst, chunk, tonemap = m_tonemap] {
                ToneMap::Process( tonemap, (uint32_t*)dst, src, chunk );
            } );
            src += chunk * 4;
            dst += chunk * 4;
            sz -= chunk;
        }
        m_td->Sync();
        return bmp;
    }
    else
    {
        return hdr->Tonemap( m_tonemap );
    }
}

std::unique_ptr<BitmapHdr> ExrLoader::LoadHdr( Colorspace colorspace )
{
    CheckPanic( m_exr, "Invalid EXR file" );
    CheckPanic( colorspace == Colorspace::BT709 || colorspace == Colorspace::BT2020, "Invalid colorspace" );

    auto dw = m_exr->dataWindow();
    auto width = dw.max.x - dw.min.x + 1;
    auto height = dw.max.y - dw.min.y + 1;

    std::vector<Imf::Rgba> hdr;
    hdr.resize( width * height );

    m_exr->setFrameBuffer( hdr.data() - dw.min.x - dw.min.y * width, 1, width );
    m_exr->readPixels( dw.min.y, dw.max.y );

    auto bmp = std::make_unique<BitmapHdr>( width, height, colorspace );

    const auto chroma = m_exr->header().findTypedAttribute<OPENEXR_IMF_INTERNAL_NAMESPACE::ChromaticitiesAttribute>( "chromaticities" );
    if( chroma )
    {
        const auto neutral = m_exr->header().findTypedAttribute<IMATH_NAMESPACE::V2f>( "adoptedNeutral" );
        const auto white = neutral ? cmsCIExyY { neutral->x, neutral->y, 1 } : cmsCIExyY { 0.3127f, 0.329f, 1 };

        const auto& cv = chroma->value();
        const cmsCIExyYTRIPLE primaries = {
            { chroma->value().red.x, chroma->value().red.y, 1 },
            { chroma->value().green.x, chroma->value().green.y, 1 },
            { chroma->value().blue.x, chroma->value().blue.y, 1 }
        };
        cmsToneCurve* linear = cmsBuildGamma( nullptr, 1 );
        cmsToneCurve* linear3[3] = { linear, linear, linear };

        auto profileIn = cmsCreateRGBProfile( &white, &primaries, linear3 );
        auto profileOut = cmsCreateRGBProfile( &white709, colorspace == Colorspace::BT709 ? &primaries709 : &primaries2020, linear3 );
        auto transform = cmsCreateTransform( profileIn, TYPE_RGBA_HALF_FLT, profileOut, TYPE_RGBA_FLT, INTENT_PERCEPTUAL, 0 );

        if( m_td )
        {
            auto src = hdr.data();
            auto dst = bmp->Data();
            auto sz = width * height;
            while( sz > 0 )
            {
                auto chunk = std::min<size_t>( sz, 16 * 1024 );
                m_td->Queue( [src, dst, chunk, transform] {
                    cmsDoTransform( transform, src, dst, chunk );
                    FixAlpha( dst, chunk );
                } );
                src += chunk;
                dst += chunk * 4;
                sz -= chunk;
            }
            m_td->Sync();
        }
        else
        {
            cmsDoTransform( transform, hdr.data(), bmp->Data(), width * height );
            FixAlpha( bmp->Data(), width * height );
        }

        cmsDeleteTransform( transform );
        cmsCloseProfile( profileIn );
        cmsCloseProfile( profileOut );
        cmsFreeToneCurve( linear );
    }
    else if( colorspace == Colorspace::BT2020 )
    {
        cmsToneCurve* linear = cmsBuildGamma( nullptr, 1 );
        cmsToneCurve* linear3[3] = { linear, linear, linear };

        auto profileIn = cmsCreateRGBProfile( &white709, &primaries709, linear3 );
        auto profileOut = cmsCreateRGBProfile( &white709, &primaries2020, linear3 );
        auto transform = cmsCreateTransform( profileIn, TYPE_RGBA_HALF_FLT, profileOut, TYPE_RGBA_FLT, INTENT_PERCEPTUAL, 0 );

        if( m_td )
        {
            auto src = hdr.data();
            auto dst = bmp->Data();
            auto sz = width * height;
            while( sz > 0 )
            {
                auto chunk = std::min<size_t>( sz, 16 * 1024 );
                m_td->Queue( [src, dst, chunk, transform] {
                    cmsDoTransform( transform, src, dst, chunk );
                    FixAlpha( dst, chunk );
                } );
                src += chunk;
                dst += chunk * 4;
                sz -= chunk;
            }
            m_td->Sync();
        }
        else
        {
            cmsDoTransform( transform, hdr.data(), bmp->Data(), width * height );
            FixAlpha( bmp->Data(), width * height );
        }

        cmsDeleteTransform( transform );
        cmsCloseProfile( profileIn );
        cmsCloseProfile( profileOut );
        cmsFreeToneCurve( linear );
    }
    else
    {
        auto dst = bmp->Data();
        auto src = hdr.data();
        auto sz = width * height;
        do
        {
            *dst++ = src->r;
            *dst++ = src->g;
            *dst++ = src->b;
            *dst++ = 1;
            src++;
        }
        while( --sz );
    }

    return bmp;
}
