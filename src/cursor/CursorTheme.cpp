#include "CursorTheme.hpp"
#include "XCursor.hpp"
#include "../util/Bitmap.hpp"
#include "../util/Panic.hpp"

extern "C" {
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_cursor.h>
}

CursorTheme::CursorTheme()
    : m_cursor( std::make_unique<XCursor>( "default" ) )
{
    CheckPanic( m_cursor->Valid(), "Cannot load default cursor" );
}

CursorTheme::~CursorTheme()
{
}

void CursorTheme::Set( wlr_cursor* cursor ) const
{
    auto img = m_cursor->Get( 24, CursorType::Default );
    CheckPanic( img, "Cursor has no 24 px pointer" );
    auto& bmp = (*img)[0];

    wlr_cursor_set_image( cursor, bmp.bitmap->Data(), bmp.bitmap->Width() * 4, bmp.bitmap->Width(), bmp.bitmap->Height(), bmp.xhot, bmp.yhot, 0 );
}
