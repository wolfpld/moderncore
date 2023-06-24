#include "CursorLogic.hpp"
#include "CursorTheme.hpp"
#include "../util/Panic.hpp"

CursorLogic::CursorLogic()
    : m_theme( std::make_unique<CursorTheme>() )
    , m_type( CursorType::Default )
    , m_frame( 0 )
    , m_needUpdate( false )
{
    CheckPanic( m_theme->Cursor().Valid(), "Cursor theme has no default cursor." );
    m_cursor = m_theme->Cursor().Get( m_theme->Size(), m_type );
}

CursorLogic::~CursorLogic()
{
}

void CursorLogic::SetCursor( CursorType type )
{
    if( type == m_type ) return;

    const auto size = m_theme->Size();
    auto& cursor = m_theme->Cursor();
    auto data = cursor.Get( size, type );

    // If the cursor is not available, use the default cursor.
    if( !data )
    {
        if( m_type == CursorType::Default ) return;
        data = cursor.Get( size, CursorType::Default );
        m_type = CursorType::Default;
    }
    else
    {
        m_type = type;
    }

    m_cursor = data;
    m_needUpdate = true;
}

bool CursorLogic::NeedUpdate()
{
    const auto ret = m_needUpdate;
    m_needUpdate = false;
    return ret;
}
