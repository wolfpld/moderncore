#include <arpa/inet.h>
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

    static const bool extensions = HasColorspaceExtensions();

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

struct Ifd
{
    uint16_t tag;
    uint16_t type;
    uint32_t count;
    uint32_t value_offset;
};

struct MpEntry
{
    uint32_t attr;
    uint32_t size;
    uint32_t offset;
    uint16_t dep1, dep2;
};

struct JpgMarker
{
    uint16_t marker;
    uint16_t size;
    uint32_t data;
};

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
    jpeg_save_markers( m_cinfo, JPEG_APP0 + 1, 0xFFFF );
    jpeg_save_markers( m_cinfo, JPEG_APP0 + 2, 0xFFFF );
    jpeg_read_header( m_cinfo, TRUE );

    m_gainMapOffset = -1;
    bool validGainMap = false;
    auto doc = LoadXmp( m_cinfo );
    if( doc )
    {
        if( auto root = doc->child( "x:xmpmeta" ).child( "rdf:RDF" ).child( "rdf:Description" ); root )
        {
            const auto hdrgmns = root.attribute( "xmlns:hdrgm" );
            const auto hdrgm = root.attribute( "hdrgm:Version" );
            const auto containerns = root.attribute( "xmlns:Container" );
            if( hdrgmns && strcmp( hdrgmns.as_string(), "http://ns.adobe.com/hdr-gain-map/1.0/" ) == 0 &&
                containerns && strcmp( containerns.as_string(), "http://ns.google.com/photos/1.0/container/" ) == 0 &&
                hdrgm && strcmp( hdrgm.as_string(), "1.0" ) == 0 )
            {
                const auto container = root.child( "Container:Directory" ).child( "rdf:Seq" ).children( "rdf:li" );
                for( auto li : container )
                {
                    if( auto item = li.child( "Container:Item" ); item )
                    {
                        if( auto semantic = item.attribute( "Item:Semantic" ); semantic )
                        {
                            if( strcmp( semantic.as_string(), "GainMap" ) == 0 )
                            {
                                validGainMap = true;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    if( validGainMap )
    {
        auto marker = m_cinfo->marker_list;
        while( marker )
        {
            if( marker->marker == JPEG_APP0 + 2 && marker->data_length > 4 && memcmp( marker->data, "MPF\0", 4 ) == 0 )
            {
                auto ptr = marker->data + 4;
                bool bigEndian = ptr[0] == 0x4D;

                uint32_t offset;
                memcpy( &offset, ptr + 4, 4 );
                if( bigEndian ) offset = ntohl( offset );

                uint16_t count;
                memcpy( &count, ptr + offset, 2 );
                if( bigEndian ) count = ntohs( count );
                offset += 2;

                int numImages = 0;
                while( count-- > 0 )
                {
                    Ifd ifd;
                    memcpy( &ifd, ptr + offset, sizeof( Ifd ) );
                    offset += sizeof( Ifd );
                    if( bigEndian )
                    {
                        ifd.tag = ntohs( ifd.tag );
                        ifd.value_offset = ntohl( ifd.value_offset );
                    }

                    if( ifd.tag == 0xB001 )
                    {
                        numImages = ifd.value_offset;
                    }
                    else if( ifd.tag == 0xB002 )
                    {
                        auto mpe = ptr + ifd.value_offset;
                        for( int i=0; i<numImages; i++ )
                        {
                            MpEntry entry;
                            memcpy( &entry, mpe, sizeof( MpEntry ) );
                            mpe += sizeof( MpEntry );

                            if( entry.attr == 0 )
                            {
                                m_gainMapOffset = bigEndian ? ntohl( entry.offset ) : entry.offset;
                                break;
                            }
                        }
                        break;
                    }
                }
                break;
            }
            marker = marker->next;
        }
    }

    // Gain map offset is relative to MFD Endian marker, the location of which is not provided by the library.
    // Do the incredibly stupid thing and search for it in raw file data.
    if( m_gainMapOffset >= 0 )
    {
        const auto filePos = ftell( *m_file );
        fseek( *m_file, 2, SEEK_SET );  // skip Start-Of-Image
        for(;;)
        {
            JpgMarker marker;
            fread( &marker, sizeof( JpgMarker ), 1, *m_file );
            marker.size = ntohs( marker.size );

            if( marker.marker == 0xE2FF && marker.size > 6 && memcmp( &marker.data, "MPF\0", 4 ) == 0 )
            {
                m_gainMapOffset += ftell( *m_file );
                mclog( LogLevel::Info, "Gain map offset: %d", m_gainMapOffset );
                break;
            }
            else
            {
                fseek( *m_file, marker.size - 6, SEEK_CUR );
            }
        }
        fseek( *m_file, filePos, SEEK_SET );
    }

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

std::unique_ptr<pugi::xml_document> JpgLoader::LoadXmp( jpeg_decompress_struct* cinfo )
{
    constexpr const char XmpSig[] = "http://ns.adobe.com/xap/1.0/";
    constexpr size_t XmpSigLen = sizeof( XmpSig );

    auto marker = cinfo->marker_list;
    while( marker )
    {
        if( marker->marker == JPEG_APP0 + 1 )
        {
            if( marker->data_length > XmpSigLen && memcmp( marker->data, XmpSig, XmpSigLen ) == 0 )
            {
                auto doc = std::make_unique<pugi::xml_document>();
                if( doc->load_buffer( marker->data + XmpSigLen, marker->data_length - XmpSigLen ) ) return doc;
                break;
            }
        }
        marker = marker->next;
    }

    return nullptr;
}
