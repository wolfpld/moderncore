#pragma once

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
    [[nodiscard]] const CursorBitmap& GetCurrentCursorFrame() const;

    bool NeedUpdate();

private:
    std::unique_ptr<CursorTheme> m_theme;
    CursorType m_type;

    const CursorData* m_cursor;
    uint32_t m_frame;
    uint32_t m_frameTime;
    uint64_t m_lastTime;

    bool m_needUpdate;
};
