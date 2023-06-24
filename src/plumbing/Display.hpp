#ifndef __DISPLAY_HPP__
#define __DISPLAY_HPP__

#include "../util/NoCopy.hpp"

extern "C" {
    struct wl_display;
};

class Display
{
public:
    Display();
    ~Display();

    NoCopy( Display );

    void Run();
    void Terminate();

    [[nodiscard]] const char* Socket() const { return m_socket; }

    [[nodiscard]] operator wl_display* () const { return m_dpy; }

private:
    wl_display* m_dpy;
    const char* m_socket;
};

#endif
