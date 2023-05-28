#include <algorithm>
#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <sys/stat.h>
#include <vector>

#include "CursorType.hpp"
#include "XCursor.hpp"
#include "../util/Bitmap.hpp"
#include "../util/FileWrapper.hpp"
#include "../util/RobinHood.hpp"

extern "C" {
#include "../../contrib/ini/ini.h"
#include <wlr/util/log.h>
}

// Based on https://www.freedesktop.org/wiki/Specifications/cursor-spec/
constexpr const char* CursorNames[] = {
    "left_ptr",         // Default
    "context-menu",     // ContextMenu
    "help",             // Help
    "pointer",          // Pointer
    "progress",         // Progress
    "wait",             // Wait
    "cell",             // Cell
    "crosshair",        // Crosshair
    "text",             // Text
    "vertical-text",    // VerticalText
    "alias",            // Alias
    "copy",             // Copy
    "dnd-move",         // Move
    "no-drop",          // NoDrop
    "not-allowed",      // NotAllowed
    "openhand",         // Grab
    "dnd-none",         // Grabbing
    "e-resize",         // EResize
    "n-resize",         // NResize
    "ne-resize",        // NEResize
    "nw-resize",        // NWResize
    "s-resize",         // SResize
    "se-resize",        // SEResize
    "sw-resize",        // SWResize
    "w-resize",         // WResize
    "ew-resize",        // EWResize
    "ns-resize",        // NSResize
    "nesw-resize",      // NESWResize
    "nwse-resize",      // NWSEResize
    "col-resize",       // ColResize
    "row-resize",       // RowResize
    "all-scroll",       // AllScroll
    "zoom-in",          // ZoomIn
    "zoom-out",         // ZoomOut
};

static_assert( sizeof( CursorNames ) / sizeof( *CursorNames ) == (int)CursorType::NUM );


static void SplitString( const char* str, std::vector<std::string>& out )
{
    while( *str == ':' ) str++;
    for(;;)
    {
        auto start = str;
        while( *str && *str != ':' ) str++;
        if( str - start > 0 ) out.emplace_back( start, str );
        while( *str == ':' ) str++;
        if( !*str ) return;
    }
}

static std::string ExpandHome( const char* path )
{
    if( path[0] != '~' ) return path;
    const auto home = getenv( "HOME" );
    if( !home ) return path;
    return std::string( home ) + ( path+1 );
}

static std::vector<std::string> GetPaths()
{
    std::vector<std::string> ret;

    auto path = getenv( "XCURSOR_PATH" );
    if( path )
    {
        SplitString( path, ret );
        for( auto& v : ret )
        {
            if( v[0] == '~' )
            {
                v = ExpandHome( v.c_str() );
            }
        }
        return ret;
    }

    path = getenv( "XDG_DATA_HOME" );
    if( path )
    {
        ret.emplace_back( ExpandHome( path ) + "/icons" );
    }

    ret.emplace_back( ExpandHome( "~/.local/share/icons" ) );
    ret.emplace_back( ExpandHome( "~/.icons" ) );
    ret.emplace_back( "/usr/share/icons" );
    ret.emplace_back( "/usr/share/pixmaps" );
    ret.emplace_back( ExpandHome( "~/.cursors" ) );
    ret.emplace_back( "/usr/share/cursors/xorg-x11" );
    ret.emplace_back( "/usr/X11R6/lib/X11/icons" );

    return ret;
}

static std::vector<std::string> GetThemeData( const char* theme )
{
    std::vector<std::string> themeData;
    static const auto paths = GetPaths();

    unordered_flat_set<std::string> done;
    std::vector<std::string> todo { theme };
    while( !todo.empty() )
    {
        const auto curr = std::move( *todo.begin() );
        todo.erase( todo.begin() );

        if( done.find( curr ) != done.end() ) continue;
        done.emplace( curr );

        for( auto& dir : paths )
        {
            auto path = dir + '/' + curr;
            struct stat st;
            if( stat( path.c_str(), &st ) == 0 && ( st.st_mode & S_IFMT ) == S_IFDIR )
            {
                const auto configPath = path + "/index.theme";
                auto config = ini_load( configPath.c_str() );
                if( config )
                {
                    auto inherit = ini_get( config, "Icon Theme", "Inherits" );
                    if( inherit )
                    {
                        todo.emplace_back( inherit );
                    }
                    ini_free( config );
                }

                path.append( "/cursors/" );
                if( stat( path.c_str(), &st ) == 0 && ( st.st_mode & S_IFMT ) == S_IFDIR )
                {
                    wlr_log( WLR_INFO, "Including cursor theme %s", curr.c_str() );
                    themeData.emplace_back( std::move( path ) );
                }
            }
        }
    }

    return themeData;
}

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

// Chunk header data is redundant, so just skip it
constexpr size_t XcursorChunkHdrSize = sizeof( uint32_t ) * 4;

constexpr uint32_t XcursorTypeImage = 0xfffd0002;

struct XcursorImage
{
    uint32_t width;
    uint32_t height;
    uint32_t xhot;
    uint32_t yhot;
    uint32_t delay;
};

static bool LoadCursor( const std::string& path, int cursorType, unordered_flat_map<uint32_t, CursorSize>& cursor )
{
    FileWrapper f( path.c_str(), "rb" );
    if( !f ) return false;

    XcursorHdr hdr;
    if( !f.Read( &hdr, sizeof( hdr ) ) || memcmp( &hdr.magic, "Xcur", 4 ) != 0 ) return false;

    std::vector<XcursorToc> toc( hdr.ntoc );
    if( !f.Read( toc.data(), sizeof( XcursorToc ) * hdr.ntoc ) ) return false;

    for( auto& v : toc )
    {
        if( v.type != XcursorTypeImage ) continue;
        fseek( f, v.pos + XcursorChunkHdrSize, SEEK_SET );
        XcursorImage img;
        if( !f.Read( &img, sizeof( XcursorImage ) ) ) return false;

        auto bitmap = std::make_shared<Bitmap>( img.width, img.height );
        if( !f.Read( bitmap->Data(), img.width * img.height * 4 ) ) return false;

        auto it = cursor.find( v.subtype );
        if( it == cursor.end() ) it = cursor.emplace( v.subtype, CursorSize {} ).first;

        it->second.type[cursorType].emplace_back( CursorBitmap { std::move( bitmap ), img.xhot, img.yhot, img.delay } );
    }

    return true;
}

XCursor::XCursor( const char* theme )
{
    const auto themeData = GetThemeData( theme );
    const auto numTypes = (int)CursorType::NUM;

    int left = numTypes;
    for( auto& td : themeData )
    {
        for( int i=0; i<numTypes; i++ )
        {
            const auto done = std::find_if( m_cursor.begin(), m_cursor.end(), [i]( const auto& v ){ return !v.second.type[i].empty(); } );
            if( done != m_cursor.end() ) continue;

            const auto path = td + CursorNames[i];
            if( LoadCursor( path, i, m_cursor ) ) left--;
            if( left == 0 ) break;
        }
        if( left == 0 ) break;
    }
    if( m_cursor.empty() ) return;

    wlr_log( WLR_INFO, "Loaded %i/%i X cursors from theme %s", numTypes - left, numTypes, theme );
}
