#include <assert.h>

#include "CursorLogic.hpp"
#include "CursorTheme.hpp"
#include "../util/Clock.hpp"

CursorLogic::CursorLogic()
    : m_theme( std::make_unique<CursorTheme>() )
    , m_type( CursorType::Default )
    , m_frame( 0 )
    , m_lastTime( GetTimeMicro() )
    , m_needUpdate( false )
{
    assert( m_theme->Cursor().Valid() );
    m_cursor = m_theme->Cursor().Get( m_theme->Size(), m_type );
    m_frameTime = m_cursor->frames.front().delay;
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
    m_frame = 0;
    m_frameTime = m_cursor->frames.front().delay;
    m_needUpdate = true;
}

[[nodiscard]] const CursorBitmap& CursorLogic::GetCurrentCursorFrame() const
{
    assert( m_cursor );
    if( m_cursor->bitmaps.size() == 1 ) return m_cursor->bitmaps.front();

    const auto animFrame = m_cursor->frames[m_frame];
    return m_cursor->bitmaps[animFrame.frame];
}

bool CursorLogic::NeedUpdate()
{
    assert( m_cursor );
    if( m_cursor->bitmaps.size() == 1 )
    {
        const auto ret = m_needUpdate;
        m_needUpdate = false;
        return ret;
    }
    else
    {
        const auto now = GetTimeMicro();
        auto delta = now - m_lastTime;
        m_lastTime = now;

        while( delta > m_frameTime )
        {
            delta -= m_frameTime;
            m_frame = ( m_frame + 1 ) % m_cursor->frames.size();
            m_frameTime = m_cursor->frames[m_frame].delay;
        }
        m_frameTime -= delta;

        m_needUpdate = false;
        return true;
    }
}
