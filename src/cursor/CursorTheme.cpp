#include <format>

#include "CursorTheme.hpp"
#include "WinCursor.hpp"
#include "XCursor.hpp"
#include "../util/Config.hpp"

CursorTheme::CursorTheme()
{
    Config config( "mouse.ini" );

    const auto themeName = config.Get( "Theme", "Name", "default" );
    m_cursor = std::make_unique<XCursor>( themeName );
    if( !m_cursor->Valid() )
    {
        m_cursor = std::make_unique<WinCursor>( themeName );
        if( !m_cursor->Valid() ) throw( CursorException( std::format( "Cannot load mouse cursor theme {}", themeName ) ) );
    }

    m_size = m_cursor->FitSize( config.Get( "Theme", "Size", 24 ) );
}

CursorTheme::~CursorTheme()
{
}
