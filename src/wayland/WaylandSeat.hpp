#pragma once

#include <memory>
#include <wayland-client.h>

#include "util/NoCopy.hpp"

#include "wayland-cursor-shape-client-protocol.h"

class WaylandDisplay;
class WaylandKeyboard;
class WaylandPointer;

class WaylandSeat
{
public:
    explicit WaylandSeat( wl_seat* seat, WaylandDisplay& dpy );
    ~WaylandSeat();

    NoCopy( WaylandSeat );

    void PointerMotion( double x, double y );
    void SetCursorShapeManager( wp_cursor_shape_manager_v1* cursorShapeManager );
    void SetDataDeviceManager( wl_data_device_manager* dataDeviceManager );

    [[nodiscard]] WaylandPointer& Pointer() { return *m_pointer; }
    [[nodiscard]] const WaylandPointer& Pointer() const { return *m_pointer; }

private:
    void Capabilities( wl_seat* seat, uint32_t caps );
    void Name( wl_seat* seat, const char* name );

    void DataOffer( wl_data_device* dev, wl_data_offer* offer );
    void DataEnter( wl_data_device* dev, uint32_t serial, wl_surface* surf, wl_fixed_t x, wl_fixed_t y, wl_data_offer* offer );
    void DataLeave( wl_data_device* dev );
    void DataMotion( wl_data_device* dev, uint32_t time, wl_fixed_t x, wl_fixed_t y );
    void DataDrop( wl_data_device* dev );
    void DataSelection( wl_data_device* dev, wl_data_offer* offer );

    wl_seat* m_seat;
    std::unique_ptr<WaylandPointer> m_pointer;
    std::unique_ptr<WaylandKeyboard> m_keyboard;

    wp_cursor_shape_manager_v1* m_cursorShapeManager = nullptr;
    wl_data_device* m_dataDevice = nullptr;

    WaylandDisplay& m_dpy;
};
