#pragma once

#include "util/NoCopy.hpp"

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
