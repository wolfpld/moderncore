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

    operator wl_display* () const { return m_dpy; }

private:
    struct wl_display* m_dpy;
};

#endif
