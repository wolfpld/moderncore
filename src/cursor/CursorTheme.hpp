#ifndef __CURSORTHEME_HPP__
#define __CURSORTHEME_HPP__

#include <memory>

extern "C" {
    struct wlr_cursor;
}

class CursorBase;

class CursorTheme
{
public:
    CursorTheme();
    ~CursorTheme();

    void Set( wlr_cursor* cursor ) const;

private:
    std::unique_ptr<CursorBase> m_cursor;
    uint32_t m_size;
};

#endif
