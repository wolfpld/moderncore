#ifndef __CURSORLOGIC_HPP__
#define __CURSORLOGIC_HPP__

#include <assert.h>
#include <memory>
#include <vector>

#include "CursorBase.hpp"
#include "CursorType.hpp"
#include "../util/NoCopy.hpp"

class CursorTheme;

class CursorLogic
{
public:
    CursorLogic();
    ~CursorLogic();

    NoCopy( CursorLogic );

    void SetCursor( CursorType type );
    [[nodiscard]] const CursorBitmap& GetCursorData() const { assert( m_cursor ); return (*m_cursor)[m_frame]; }

    bool NeedUpdate();

private:
    std::unique_ptr<CursorTheme> m_theme;
    CursorType m_type;

    const std::vector<CursorBitmap>* m_cursor;
    uint32_t m_frame;

    bool m_needUpdate;
};

#endif
