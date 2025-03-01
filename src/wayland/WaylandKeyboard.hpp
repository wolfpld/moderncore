#pragma once

#include <wayland-client.h>

#include "util/NoCopy.hpp"

class WaylandKeyboard
{
public:
    explicit WaylandKeyboard( wl_keyboard* keyboard );
    ~WaylandKeyboard();

    NoCopy( WaylandKeyboard );

    [[nodiscard]] wl_surface* ActiveWindow() const { return m_activeWindow; }

private:
    wl_keyboard* m_keyboard;

    void Keymap( wl_keyboard* kbd, uint32_t format, int32_t fd, uint32_t size );
    void Enter( wl_keyboard* kbd, uint32_t serial, wl_surface* surf, wl_array* keys );
    void Leave( wl_keyboard* kbd, uint32_t serial, wl_surface* surf );
    void Key( wl_keyboard* kbd, uint32_t serial, uint32_t time, uint32_t key, uint32_t state );
    void Modifiers( wl_keyboard* kbd, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group );
    void RepeatInfo( wl_keyboard* kbd, int32_t rate, int32_t delay );

    wl_surface* m_activeWindow = nullptr;
};
