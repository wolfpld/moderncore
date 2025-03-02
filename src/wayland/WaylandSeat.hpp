#pragma once

#include <memory>
#include <string>
#include <wayland-client.h>

#include "util/NoCopy.hpp"
#include "util/RobinHood.hpp"

#include "wayland-cursor-shape-client-protocol.h"

class WaylandDataOffer;
class WaylandDisplay;
class WaylandKeyboard;
class WaylandPointer;
class WaylandWindow;

class WaylandSeat
{
    friend class WaylandKeyboard;

public:
    explicit WaylandSeat( wl_seat* seat, WaylandDisplay& dpy );
    ~WaylandSeat();

    NoCopy( WaylandSeat );

    void SetCursorShapeManager( wp_cursor_shape_manager_v1* cursorShapeManager );
    void SetDataDeviceManager( wl_data_device_manager* dataDeviceManager );

    void AddWindow( WaylandWindow* window );
    void RemoveWindow( WaylandWindow* window );

    [[nodiscard]] WaylandPointer& Pointer() { return *m_pointer; }
    [[nodiscard]] const WaylandPointer& Pointer() const { return *m_pointer; }

    [[nodiscard]] int GetClipboard( const char* mime );
    void AcceptDndMime( const char* mime );
    void FinishDnd( int fd );

private:
    void KeyboardLeave( wl_surface* surf );
    void KeyEntered( wl_surface* surf, const char* key, int mods );

    void Capabilities( wl_seat* seat, uint32_t caps );
    void Name( wl_seat* seat, const char* name );

    void DataOffer( wl_data_device* dev, wl_data_offer* offer );
    void DataEnter( wl_data_device* dev, uint32_t serial, wl_surface* surf, wl_fixed_t x, wl_fixed_t y, wl_data_offer* offer );
    void DataLeave( wl_data_device* dev );
    void DataMotion( wl_data_device* dev, uint32_t time, wl_fixed_t x, wl_fixed_t y );
    void DataDrop( wl_data_device* dev );
    void DataSelection( wl_data_device* dev, wl_data_offer* offer );

    [[nodiscard]] WaylandWindow* GetFocusedWindow() const;
    [[nodiscard]] WaylandWindow* GetWindow( wl_surface* surf ) const;

    wl_seat* m_seat;
    std::unique_ptr<WaylandPointer> m_pointer;
    std::unique_ptr<WaylandKeyboard> m_keyboard;

    wp_cursor_shape_manager_v1* m_cursorShapeManager = nullptr;
    wl_data_device* m_dataDevice = nullptr;

    std::unique_ptr<WaylandDataOffer> m_nextOffer;
    std::unique_ptr<WaylandDataOffer> m_selectionOffer;
    std::unique_ptr<WaylandDataOffer> m_dndOffer;
    uint32_t m_dndSerial = 0;
    wl_surface* m_dndSurface = nullptr;
    std::string m_dndMime;

    unordered_flat_map<wl_surface*, WaylandWindow*> m_windows;
    unordered_flat_map<int, std::unique_ptr<WaylandDataOffer>> m_pendingDnd;

    WaylandDisplay& m_dpy;
};
