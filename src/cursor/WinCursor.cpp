#include <alloca.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <vector>

#include "CursorType.hpp"
#include "WinCursor.hpp"
#include "../util/Bitmap.hpp"
#include "../util/FileWrapper.hpp"
#include "../util/Home.hpp"
#include "../util/Logs.hpp"

extern "C" {
#include "../../contrib/ini/ini.h"
}

constexpr const char* CursorNames[] = {
    "Default",
    "ContextMenu",
    "Help",
    "Pointer",
    "Progress",
    "Wait",
    "Cell",
    "Crosshair",
    "Text",
    "VerticalText",
    "Alias",
    "Copy",
    "Move",
    "NoDrop",
    "NotAllowed",
    "Grab",
    "Grabbing",
    "EResize",
    "NResize",
    "NEResize",
    "NWResize",
    "SResize",
    "SEResize",
    "SWResize",
    "WResize",
    "EWResize",
    "NSResize",
    "NESWResize",
    "NWSEResize",
    "ColResize",
    "RowResize",
    "AllScroll",
    "ZoomIn",
    "ZoomOut",
};

static_assert( sizeof( CursorNames ) / sizeof( *CursorNames ) == (int)CursorType::NUM );


static std::vector<std::string> GetPaths()
{
    std::vector<std::string> ret;

    auto path = getenv( "XDG_DATA_HOME" );
    if( path )
    {
        ret.emplace_back( ExpandHome( path ) + "/ModernCore/cursors/" );
    }

    ret.emplace_back( ExpandHome( "~/.local/share/ModernCore/cursors/" ) );
    ret.emplace_back( "/usr/share/ModernCore/cursors/" );

    return ret;
}

struct IconHeader
{
    uint16_t reserved;
    uint16_t type;
    uint16_t count;
};

struct IconEntry
{
    uint8_t width;
    uint8_t height;
    uint8_t numColors;
    uint8_t reserved;
    uint16_t xhot;
    uint16_t yhot;
    uint32_t dataSize;
    uint32_t dataOffset;
};

struct BitmapInfoHeader
{
    uint32_t size;
    int32_t width;
    int32_t height;
    uint16_t planes;
    uint16_t bitCount;
    uint32_t compression;
    uint32_t imageSize;
    int32_t xPelsPerMeter;
    int32_t yPelsPerMeter;
    uint32_t clrUsed;
    uint32_t clrImportant;
};

// TODO: Support 16-bit color and PNG payload.
static bool LoadCursor( CursorType cursorType, unordered_flat_map<uint32_t, CursorSize>& cursor, FileWrapper& f, size_t offset )
{
    IconHeader hdr;
    if( !f.Read( &hdr, sizeof( hdr ) ) ) return false;
    if( hdr.reserved != 0 || hdr.type != 2 ) return false;

    std::vector<IconEntry> entries( hdr.count );
    if( !f.Read( entries.data(), sizeof( IconEntry ) * hdr.count ) ) return false;

    const auto type = (int)cursorType;
    for( auto& v : entries )
    {
        if( v.reserved != 0 ) return false;

        BitmapInfoHeader bmpHdr;
        fseek( f, offset + v.dataOffset, SEEK_SET );
        if( !f.Read( &bmpHdr, sizeof( bmpHdr ) ) ) return false;
        if( bmpHdr.size != sizeof( bmpHdr ) ) return false;
        if( bmpHdr.compression != 0 ) return false;

        const auto w = bmpHdr.width;
        const auto h = abs( bmpHdr.height / 2 );

        if( (w*h) % 8 != 0 ) return false;

        auto bitmap = std::make_unique<Bitmap>( w, h );

        switch( bmpHdr.bitCount )
        {
        case 1:
        {
            int colors = bmpHdr.clrUsed ? bmpHdr.clrUsed : (1 << bmpHdr.bitCount);
            std::vector<uint32_t> palette( colors );
            if( !f.Read( palette.data(), sizeof( uint32_t ) * colors ) ) return false;

            auto msz = w*h/8;
            auto data = (uint8_t*)alloca( msz );
            if( !f.Read( data, msz ) ) return false;
            auto dst = bitmap->Data();
            while( msz > 0 )
            {
                auto px = *data++;
                for( int i=0; i<8; i++ )
                {
                    memcpy( dst, &palette[px >> 7], 4 );
                    dst += 4;
                    px <<= 1;
                }
                msz--;
            }
            break;
        }
        case 4:
        case 8:
        {
            int colors = bmpHdr.clrUsed ? bmpHdr.clrUsed : (1 << bmpHdr.bitCount);
            std::vector<uint32_t> palette( colors );
            if( !f.Read( palette.data(), sizeof( uint32_t ) * colors ) ) return false;

            auto dst = bitmap->Data();
            auto cnt = w*h;
            if( bmpHdr.bitCount == 4 )
            {
                while( cnt > 0 )
                {
                    uint8_t px;
                    if( !f.Read( &px, 1 ) ) return false;
                    memcpy( dst, &palette[px >> 4], 4 );
                    dst += 4;
                    memcpy( dst, &palette[px & 0xF], 4 );
                    dst += 4;
                    cnt -= 2;
                }
            }
            else
            {
                while( cnt > 0 )
                {
                    uint8_t px;
                    if( !f.Read( &px, 1 ) ) return false;
                    memcpy( dst, &palette[px], 4 );
                    dst += 4;
                    cnt--;
                }
            }
            break;
        }
        case 16:
            return false;
        case 32:
            if( !f.Read( bitmap->Data(), w*h*4 ) ) return false;
            break;
        default:
            return false;
        }

        const auto stride = ( w + 31 ) / 32 * 32;
        const auto msz = stride*h/8;
        auto mask = (uint8_t*)alloca( msz );
        if( !f.Read( mask, msz ) ) return false;

        auto dst = bitmap->Data();
        for( int y=0; y<h; y++ )
        {
            for( int x=0; x<w/8; x++ )
            {
                auto px = *mask++;
                for( int i=0; i<8; i++ )
                {
                    if( (px & 0x80) == 0 )
                    {
                        *(dst+3) = 0xFF;
                    }
                    else
                    {
                        memset( dst, 0, 4 );
                    }
                    dst += 4;
                    px <<= 1;
                }
            }
            mask += ( stride - w ) / 8;
        }

        if( bmpHdr.height > 0 ) bitmap->FlipVertical();

        auto it = cursor.find( w );
        if( it == cursor.end() ) it = cursor.emplace( w, CursorSize {} ).first;

        it->second.type[type].emplace_back( CursorBitmap { std::move( bitmap ), v.xhot, v.yhot, 0 } );
    }

    return true;
}

struct RiffChunk
{
    uint32_t fourcc;
    uint32_t size;
};

struct AniHeader
{
    uint32_t size;
    uint32_t numFrames;
    uint32_t numSteps;
    uint32_t width;
    uint32_t height;
    uint32_t bitCount;
    uint32_t numPlanes;
    uint32_t displayRate;
    uint32_t flags;
};

// TODO: Handle rate and seq tables.
static bool LoadCursor( const std::string& path, CursorType cursorType, unordered_flat_map<uint32_t, CursorSize>& cursor )
{
    FileWrapper f( path.c_str(), "rb" );
    if( !f ) return false;

    uint32_t magic;
    if( !f.Read( &magic, 4 ) ) return false;
    if( memcmp( &magic, "RIFF", 4 ) != 0 )
    {
        fseek( f, 0, SEEK_SET );
        return LoadCursor( cursorType, cursor, f, 0 );
    }

    if( fseek( f, 4, SEEK_CUR ) != 0 ) return false;
    if( !f.Read( &magic, 4 ) ) return false;
    if( memcmp( &magic, "ACON", 4 ) != 0 ) return false;

    bool gotFrames = false;
    bool gotAniHeader = false;
    AniHeader aniHeader;

    for(;;)
    {
        RiffChunk chunk;
        if( !f.Read( &chunk, sizeof( RiffChunk ) ) ) break;

        if( memcmp( &chunk.fourcc, "LIST", 4 ) == 0 )
        {
            uint32_t hdrid;
            if( !f.Read( &hdrid, 4 ) ) return false;
            if( memcmp( &hdrid, "fram", 4 ) != 0 )
            {
                fseek( f, chunk.size - 4, SEEK_CUR );
                continue;
            }

            if( !gotAniHeader ) return false;
            for( uint32_t i=0; i<aniHeader.numFrames; i++ )
            {
                RiffChunk icon;
                if( !f.Read( &icon, sizeof( RiffChunk ) ) ) return false;
                if( memcmp( &icon.fourcc, "icon", 4 ) != 0 ) return false;

                if( !LoadCursor( cursorType, cursor, f, ftell( f ) ) ) return false;
            }
            gotFrames = true;
        }
        else if( memcmp( &chunk.fourcc, "anih", 4 ) == 0 )
        {
            if( chunk.size != sizeof( AniHeader ) ) return false;
            if( !f.Read( &aniHeader, sizeof( AniHeader ) ) ) return false;

            // Bit 1 is the IconFlag value. Zero indicated images are raw data.
            // This is not supported.
            if( (aniHeader.flags & 1) == 0 ) return false;

            gotAniHeader = true;
        }
        else if( memcmp( &chunk.fourcc, "rate", 4 ) == 0 )
        {
            fseek( f, chunk.size, SEEK_CUR );
        }
        else if( memcmp( &chunk.fourcc, "seq ", 4 ) == 0 )
        {
            fseek( f, chunk.size, SEEK_CUR );
        }
        else
        {
            fseek( f, chunk.size, SEEK_CUR );
        }
    }

    return gotFrames;
}

WinCursor::WinCursor( const char* theme )
{
    int numCursors = (int)CursorType::NUM;
    static const auto paths = GetPaths();
    for( auto& dir : paths )
    {
        const auto path = dir + theme + '/';
        const auto iniPath = path + "theme.ini";
        auto ini = ini_load( iniPath.c_str() );
        if( ini )
        {
            auto defaultName = ini_get( ini, "Theme", "Default" );
            if( defaultName )
            {
                assert( m_cursor.empty() );
                if( LoadCursor( path + defaultName, CursorType::Default, m_cursor ) )
                {
                    auto it = m_cursor.find( 32 );
                    assert( it != m_cursor.end() );

                    for( int i=1; i<(int)CursorType::NUM; i++ )
                    {
                        auto name = ini_get( ini, "Theme", CursorNames[i] );
                        if( !(name && LoadCursor( path + name, (CursorType)i, m_cursor ) ) )
                        {
                            numCursors--;
                        }
                    }
                }
            }
            ini_free( ini );
        }
        if( !m_cursor.empty() ) break;
    }
    if( m_cursor.empty() ) return;

    CalcSizes();

    mclog( LogLevel::Info, "Loaded %i Windows cursors from theme %s", numCursors, theme );
}
