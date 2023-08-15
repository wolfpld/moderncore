#pragma once

#include <wayland-client.h>

#include "../../util/NoCopy.hpp"

class WaylandKeyboard
{
public:
    explicit WaylandKeyboard( wl_keyboard* keyboard );
    ~WaylandKeyboard();

    NoCopy( WaylandKeyboard );

private:
    wl_keyboard* m_keyboard;

    void KeyboardKeymap( wl_keyboard* kbd, uint32_t format, int32_t fd, uint32_t size );
    void KeyboardEnter( wl_keyboard* kbd, uint32_t serial, wl_surface* surf, wl_array* keys );
    void KeyboardLeave( wl_keyboard* kbd, uint32_t serial, wl_surface* surf );
    void KeyboardKey( wl_keyboard* kbd, uint32_t serial, uint32_t time, uint32_t key, uint32_t state );
    void KeyboardModifiers( wl_keyboard* kbd, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group );
    void KeyboardRepeatInfo( wl_keyboard* kbd, int32_t rate, int32_t delay );
};
