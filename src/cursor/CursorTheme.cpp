#include "CursorTheme.hpp"
#include "../util/Panic.hpp"

extern "C" {
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_xcursor_manager.h>
}

CursorTheme::CursorTheme()
    : m_manager( wlr_xcursor_manager_create( nullptr, 24 ) )
{
    CheckPanic( m_manager, "Failed to create wlr_xcursor_manager!" );

    wlr_xcursor_manager_load( m_manager, 1 );
}

CursorTheme::~CursorTheme()
{
    wlr_xcursor_manager_destroy( m_manager );
}

void CursorTheme::Set( wlr_cursor* cursor ) const
{
    wlr_xcursor_manager_set_cursor_image( m_manager, "left_ptr", cursor );
}
