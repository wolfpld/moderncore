#ifndef __CURSOR_HPP__
#define __CURSOR_HPP__

extern "C" {
    struct wlr_cursor;
    struct wlr_xcursor_manager;
};

class Cursor
{
public:
    Cursor();
    ~Cursor();

private:
    struct wlr_cursor* m_cursor;
    struct wlr_xcursor_manager* m_manager;
};

#endif
