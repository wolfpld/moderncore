#pragma once

#include <xkbcommon/xkbcommon.h>
#include <wayland-client.h>

#include "util/NoCopy.hpp"

class WaylandSeat;
struct xkb_compose_table;
struct xkb_compose_state;

class WaylandKeyboard
{
public:
    WaylandKeyboard( wl_keyboard* keyboard, WaylandSeat& seat );
    ~WaylandKeyboard();

    NoCopy( WaylandKeyboard );

    [[nodiscard]] wl_surface* ActiveWindow() const { return m_activeWindow; }

private:
    wl_keyboard* m_keyboard;
    WaylandSeat& m_seat;

    void Keymap( wl_keyboard* kbd, uint32_t format, int32_t fd, uint32_t size );
    void Enter( wl_keyboard* kbd, uint32_t serial, wl_surface* surf, wl_array* keys );
    void Leave( wl_keyboard* kbd, uint32_t serial, wl_surface* surf );
    void Key( wl_keyboard* kbd, uint32_t serial, uint32_t time, uint32_t key, uint32_t state );
    void Modifiers( wl_keyboard* kbd, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group );
    void RepeatInfo( wl_keyboard* kbd, int32_t rate, int32_t delay );

    wl_surface* m_activeWindow = nullptr;

    xkb_context* m_ctx;
    xkb_keymap* m_keymap = nullptr;
    xkb_state* m_state = nullptr;
    xkb_compose_table* m_composeTable = nullptr;
    xkb_compose_state* m_composeState = nullptr;

    xkb_mod_index_t m_ctrl, m_alt, m_shift, m_super;
};
