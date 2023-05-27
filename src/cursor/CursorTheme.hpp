#ifndef __CURSORTHEME_HPP__
#define __CURSORTHEME_HPP__

extern "C" {
    struct wlr_cursor;
    struct wlr_xcursor_manager;
}

class CursorTheme
{
public:
    CursorTheme();
    ~CursorTheme();

    void Set( wlr_cursor* cursor ) const;

private:
    wlr_xcursor_manager* m_manager;
};

#endif
