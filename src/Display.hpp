#ifndef __DISPLAY_HPP__
#define __DISPLAY_HPP__

extern "C" {
    struct wl_display;
};

class Display
{
public:
    Display();
    ~Display();

private:
    struct wl_display* m_dpy;
};

#endif
