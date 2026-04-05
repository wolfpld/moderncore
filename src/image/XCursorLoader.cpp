#include <string.h>
#include <vector>

#include "XCursorLoader.hpp"
#include "util/Bitmap.hpp"
#include "util/FileWrapper.hpp"

struct XcursorHdr
{
    uint32_t magic;
    uint32_t header;
    uint32_t version;
    uint32_t ntoc;
};

struct XcursorToc
{
    uint32_t type;
    uint32_t subtype;
    uint32_t pos;
};

struct XcursorImage
{
    uint32_t width;
    uint32_t height;
    uint32_t xhot;
    uint32_t yhot;
    uint32_t delay;
};

XCursorLoader::XCursorLoader( std::shared_ptr<FileWrapper> file )
    : m_file( std::move( file ) )
{
    fseek( *m_file, 0, SEEK_SET );
    XcursorHdr hdr;
    m_valid = m_file->Read( &hdr, sizeof( hdr ) ) && memcmp( &hdr.magic, "Xcur", 4 ) == 0;
    if( !m_valid ) return;

    m_ntoc = hdr.ntoc;
}

bool XCursorLoader::IsValidSignature( const uint8_t* buf, size_t size )
{
    if( size < 4 ) return false;
    return memcmp( buf, "Xcur", 4 ) == 0;
}

bool XCursorLoader::IsValid() const
{
    return m_valid;
}

std::unique_ptr<Bitmap> XCursorLoader::Load()
{
    constexpr size_t XcursorChunkHdrSize = sizeof( uint32_t ) * 4;
    constexpr uint32_t XcursorTypeImage = 0xfffd0002;

    std::vector<XcursorToc> toc( m_ntoc );
    if( !m_file->Read( toc.data(), sizeof( XcursorToc ) * m_ntoc ) ) return nullptr;

    uint32_t best = 0;
    size_t bestScore = 0;

    for( uint32_t i = 0; i < m_ntoc; i++ )
    {
        auto& v = toc[i];
        if( v.type != XcursorTypeImage ) continue;
        fseek( *m_file, v.pos + XcursorChunkHdrSize, SEEK_SET );
        XcursorImage img;
        if( !m_file->Read( &img, sizeof( XcursorImage ) ) ) return nullptr;

        const auto score = img.width * img.height;
        if( score > bestScore )
        {
            best = i;
            bestScore = score;
        }
    }

    if( bestScore == 0 ) return nullptr;

    auto& v = toc[best];
    fseek( *m_file, v.pos + XcursorChunkHdrSize, SEEK_SET );
    XcursorImage img;
    if( !m_file->Read( &img, sizeof( XcursorImage ) ) ) return nullptr;

    auto bitmap = std::make_unique<Bitmap>( img.width, img.height );
    if( !m_file->Read( bitmap->Data(), img.width * img.height * 4 ) ) return nullptr;

    bitmap->BgrToRgb();
    return bitmap;
}
