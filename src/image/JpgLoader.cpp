#include <stdio.h>
#include <jpeglib.h>
#include <lcms2.h>
#include <libexif/exif-data.h>
#include <setjmp.h>
#include <stdint.h>
#include <string.h>

#include "JpgLoader.hpp"
#include "util/Bitmap.hpp"
#include "util/EmbedData.hpp"
#include "util/FileBuffer.hpp"
#include "util/FileWrapper.hpp"
#include "util/Panic.hpp"

#include "data/CmykIcm.hpp"

JpgLoader::JpgLoader( std::shared_ptr<FileWrapper> file )
    : m_file( std::move( file ) )
    , m_cinfo( nullptr )
    , m_iccData( nullptr )
{
    fseek( *m_file, 0, SEEK_SET );
    uint8_t hdr[2];
    m_valid = fread( hdr, 1, 2, *m_file ) == 2 && hdr[0] == 0xFF && hdr[1] == 0xD8;
}

JpgLoader::~JpgLoader()
{
    free( m_iccData );
    if( m_cinfo ) jpeg_destroy_decompress( m_cinfo );
}

bool JpgLoader::IsValidSignature( const uint8_t* buf, size_t size )
{
    if( size < 2 ) return false;
    if( buf[0] != 0xFF || buf[1] != 0xD8 ) return false;
    return true;
}

bool JpgLoader::IsValid() const
{
    return m_valid;
}

struct JpgErrorMgr
{
    jpeg_error_mgr pub;
    jmp_buf setjmp_buffer;
};

namespace
{
bool HasColorspaceExtensions()
{
#ifdef JCS_EXTENSIONS
    jpeg_compress_struct cinfo;
    JpgErrorMgr jerr;

    cinfo.err = jpeg_std_error( &jerr.pub );

    if( setjmp( jerr.setjmp_buffer ) )
    {
        jpeg_destroy_compress( &cinfo );
        return false;
    }

    jpeg_create_compress( &cinfo );
    cinfo.input_components = 3;
    jpeg_set_defaults( &cinfo );
    cinfo.in_color_space = JCS_EXT_RGB;
    jpeg_default_colorspace( &cinfo );
    jpeg_destroy_compress( &cinfo );

    return true;
#else
    return false;
#endif
}
}

std::unique_ptr<Bitmap> JpgLoader::Load()
{
    if( !m_cinfo && !Open() ) return nullptr;

    const bool extensions = HasColorspaceExtensions();

    JpgErrorMgr jerr;
    m_cinfo->err = jpeg_std_error( &jerr.pub );
    jerr.pub.error_exit = []( j_common_ptr cinfo ) { longjmp( ((JpgErrorMgr*)cinfo->err)->setjmp_buffer, 1 ); };
    if( setjmp( jerr.setjmp_buffer ) ) return nullptr;

    const bool cmyk = m_cinfo->jpeg_color_space == JCS_CMYK || m_cinfo->jpeg_color_space == JCS_YCCK;
#ifdef JCS_EXTENSIONS
    if( extensions && !cmyk ) m_cinfo->out_color_space = JCS_EXT_RGBX;
#endif
    jpeg_start_decompress( m_cinfo );

    auto bmp = std::make_unique<Bitmap>( m_cinfo->output_width, m_cinfo->output_height, m_orientation );
    auto ptr = bmp->Data();

    unsigned int iccSz;
    jpeg_read_icc_profile( m_cinfo, &m_iccData, &iccSz );

    if( cmyk || extensions )
    {
        while( m_cinfo->output_scanline < m_cinfo->output_height )
        {
            jpeg_read_scanlines( m_cinfo, &ptr, 1 );
            ptr += m_cinfo->output_width * 4;
        }
    }
    else
    {
        auto row = new uint8_t[m_cinfo->output_width * 3 + 1];
        while( m_cinfo->output_scanline < m_cinfo->output_height )
        {
            jpeg_read_scanlines( m_cinfo, &row, 1 );
            for( int i=0; i<m_cinfo->output_width; i++ )
            {
                uint32_t col;
                memcpy( &col, row + i * 3, 4 );
                col |= 0xFF000000;
                memcpy( ptr, &col, 4 );
                ptr += 4;
            }
        }
        delete[] row;
    }

    if( cmyk )
    {
        cmsHPROFILE profileIn;

        if( m_iccData )
        {
            mclog( LogLevel::Info, "ICC profile size: %u", iccSz );
            profileIn = cmsOpenProfileFromMem( m_iccData, iccSz );
        }
        else
        {
            mclog( LogLevel::Info, "No ICC profile found, using default" );
            Unembed( CmykIcm );
            profileIn = cmsOpenProfileFromMem( CmykIcm->data(), CmykIcm->size() );
        }

        auto profileOut = cmsCreate_sRGBProfile();
        auto transform = cmsCreateTransform( profileIn, TYPE_CMYK_8_REV, profileOut, TYPE_RGBA_8, INTENT_PERCEPTUAL, 0 );

        if( transform )
        {
            cmsDoTransform( transform, bmp->Data(), bmp->Data(), bmp->Width() * bmp->Height() );
            cmsDeleteTransform( transform );
        }

        cmsCloseProfile( profileOut );
        cmsCloseProfile( profileIn );
    }
    else if( m_iccData )
    {
        mclog( LogLevel::Info, "ICC profile size: %u", iccSz );

        auto profileIn = cmsOpenProfileFromMem( m_iccData, iccSz );
        auto profileOut = cmsCreate_sRGBProfile();
        auto transform = cmsCreateTransform( profileIn, TYPE_RGBA_8, profileOut, TYPE_RGBA_8, INTENT_PERCEPTUAL, 0 );

        if( transform )
        {
            cmsDoTransform( transform, bmp->Data(), bmp->Data(), bmp->Width() * bmp->Height() );
            cmsDeleteTransform( transform );
        }

        cmsCloseProfile( profileOut );
        cmsCloseProfile( profileIn );
    }

    jpeg_finish_decompress( m_cinfo );

    bmp->SetAlpha( 0xFF );
    return bmp;
}

bool JpgLoader::Open()
{
    CheckPanic( m_valid, "Invalid JPEG file" );
    CheckPanic( !m_cinfo, "Already opened" );

    fseek( *m_file, 0, SEEK_SET );
    m_orientation = LoadOrientation();

    m_cinfo = new jpeg_decompress_struct();

    JpgErrorMgr jerr;
    m_cinfo->err = jpeg_std_error( &jerr.pub );
    jerr.pub.error_exit = []( j_common_ptr cinfo ) { longjmp( ((JpgErrorMgr*)cinfo->err)->setjmp_buffer, 1 ); };
    if( setjmp( jerr.setjmp_buffer ) ) return false;

    jpeg_create_decompress( m_cinfo );
    jpeg_stdio_src( m_cinfo, *m_file );
    jpeg_save_markers( m_cinfo, JPEG_APP0 + 2, 0xFFFF );
    jpeg_read_header( m_cinfo, TRUE );

    return true;
}

int JpgLoader::LoadOrientation()
{
    int orientation = 0;

    FileBuffer buf( m_file );
    auto exif = exif_data_new_from_data( (const unsigned char*)buf.data(), buf.size() );

    if( exif )
    {
        ExifEntry* entry = exif_content_get_entry( exif->ifd[EXIF_IFD_0], EXIF_TAG_ORIENTATION );
        if( entry )
        {
            orientation = exif_get_short( entry->data, exif_data_get_byte_order( exif ) );
            mclog( LogLevel::Info, "JPEG orientation: %d", orientation );
        }
        exif_data_free( exif );
    }

    return orientation;
}
