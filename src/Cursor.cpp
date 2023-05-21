#include "Cursor.hpp"
#include "Panic.hpp"

extern "C" {
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xcursor_manager.h>
};

Cursor::Cursor()
    : m_cursor( wlr_cursor_create() )
    , m_manager( wlr_xcursor_manager_create( nullptr, 24 ) )
{
    CheckPanic( m_cursor, "Failed to create wlr_cursor!" );
    CheckPanic( m_manager, "Failed to create wlr_xcursor_manager!" );

    wlr_xcursor_manager_load( m_manager, 1 );
}

Cursor::~Cursor()
{
    wlr_xcursor_manager_destroy( m_manager );
    wlr_cursor_destroy( m_cursor );
}
