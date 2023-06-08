#include "CursorTheme.hpp"
#include "WinCursor.hpp"
#include "XCursor.hpp"
#include "../util/Bitmap.hpp"
#include "../util/Config.hpp"
#include "../util/Panic.hpp"

CursorTheme::CursorTheme()
{
    Config config( "mouse.ini" );

    const auto themeName = config.Get( "Theme", "Name", "default" );
    m_cursor = std::make_unique<XCursor>( themeName );
    if( !m_cursor->Valid() )
    {
        m_cursor = std::make_unique<WinCursor>( themeName );
    }
    CheckPanic( m_cursor->Valid(), "Cannot load mouse cursor theme" );

    m_size = m_cursor->FitSize( config.Get( "Theme", "Size", 24 ) );
}

CursorTheme::~CursorTheme()
{
}
