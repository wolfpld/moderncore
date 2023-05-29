#include "CursorTheme.hpp"
#include "XCursor.hpp"
#include "../util/Bitmap.hpp"
#include "../util/Config.hpp"
#include "../util/Panic.hpp"

extern "C" {
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_cursor.h>
}

CursorTheme::CursorTheme()
{
    Config config( "mouse.ini" );

    m_cursor = std::make_unique<XCursor>( config.Get( "Theme", "Name", "default" ) );
    CheckPanic( m_cursor->Valid(), "Cannot load default cursor" );

    m_size = m_cursor->FitSize( config.Get( "Theme", "Size", 24 ) );
}

CursorTheme::~CursorTheme()
{
}

void CursorTheme::Set( wlr_cursor* cursor ) const
{
    auto img = m_cursor->Get( m_size, CursorType::Default );
    auto& bmp = (*img)[0];

    wlr_cursor_set_image( cursor, bmp.bitmap->Data(), bmp.bitmap->Width() * 4, bmp.bitmap->Width(), bmp.bitmap->Height(), bmp.xhot, bmp.yhot, 0 );
}
