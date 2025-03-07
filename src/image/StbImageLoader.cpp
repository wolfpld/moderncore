#include <lcms2.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_JPEG
#define STBI_NO_PNG
#include <stb_image.h>

#include "StbImageLoader.hpp"
#include "util/Bitmap.hpp"
#include "util/BitmapHdr.hpp"
#include "util/FileWrapper.hpp"
#include "util/Panic.hpp"

StbImageLoader::StbImageLoader( std::shared_ptr<FileWrapper> file )
    : m_file( std::move( file ) )
{
    fseek( *m_file, 0, SEEK_SET );
    int w, h, comp;
    m_valid = stbi_info_from_file( *m_file, &w, &h, &comp ) == 1;
    m_hdr = stbi_is_hdr_from_file( *m_file );
}

bool StbImageLoader::IsValid() const
{
    return m_valid;
}

bool StbImageLoader::IsHdr()
{
    return m_hdr;
}

bool StbImageLoader::PreferHdr()
{
    return true;
}

std::unique_ptr<Bitmap> StbImageLoader::Load()
{
    CheckPanic( m_valid, "Invalid stb_image file" );

    fseek( *m_file, 0, SEEK_SET );

    int w, h, comp;
    auto data = stbi_load_from_file( *m_file, &w, &h, &comp, 4 );
    if( data == nullptr ) return nullptr;

    auto bmp = std::make_unique<Bitmap>( w, h );
    memcpy( bmp->Data(), data, w * h * 4 );

    stbi_image_free( data );
    return bmp;
}

std::unique_ptr<BitmapHdr> StbImageLoader::LoadHdr( Colorspace colorspace )
{
    CheckPanic( m_valid, "Invalid stb_image file" );
    if( !m_hdr ) return nullptr;

    fseek( *m_file, 0, SEEK_SET );

    int w, h, comp;
    auto data = stbi_loadf_from_file( *m_file, &w, &h, &comp, 4 );
    if( data == nullptr ) return nullptr;

    auto hdr = std::make_unique<BitmapHdr>( w, h, colorspace );
    if( colorspace == Colorspace::BT709 )
    {
        memcpy( hdr->Data(), data, w * h * 4 * sizeof( float ) );
    }
    else if( colorspace == Colorspace::BT2020 )
    {
        cmsToneCurve* linear = cmsBuildGamma( nullptr, 1 );
        cmsToneCurve* linear3[3] = { linear, linear, linear };

        auto profileIn = cmsCreateRGBProfile( &white709, &primaries709, linear3 );
        auto profileOut = cmsCreateRGBProfile( &white709, &primaries2020, linear3 );
        auto transform = cmsCreateTransform( profileIn, TYPE_RGBA_FLT, profileOut, TYPE_RGBA_FLT, INTENT_PERCEPTUAL, 0 );

        cmsDoTransform( transform, data, hdr->Data(), w * h );

        cmsDeleteTransform( transform );
        cmsCloseProfile( profileIn );
        cmsCloseProfile( profileOut );
        cmsFreeToneCurve( linear );

        auto ptr = hdr->Data() + 3;
        auto sz = w * h;
        do
        {
            *ptr = 1;
            ptr += 4;
        }
        while( --sz );
    }
    else
    {
        Panic( "Invalid colorspace" );
    }

    stbi_image_free( data );
    return hdr;
}
