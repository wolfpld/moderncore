#ifndef __WAYLANDPOINTER_HPP__
#define __WAYLANDPOINTER_HPP__

#include <wayland-client.h>

class WaylandPointer
{
public:
    explicit WaylandPointer( wl_pointer* pointer );
    ~WaylandPointer();

private:
    wl_pointer* m_pointer;

    void PointerEnter( wl_pointer* pointer, uint32_t serial, wl_surface* surf, wl_fixed_t sx, wl_fixed_t sy );
    void PointerLeave( wl_pointer* pointer, uint32_t serial, wl_surface* surf );
    void PointerMotion( wl_pointer* pointer, uint32_t time, wl_fixed_t sx, wl_fixed_t sy );
    void PointerButton( wl_pointer* pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state );
    void PointerAxis( wl_pointer* pointer, uint32_t time, uint32_t axis, wl_fixed_t value );
    void PointerFrame( wl_pointer* pointer );
    void PointerAxisSource( wl_pointer* pointer, uint32_t source );
    void PointerAxisStop( wl_pointer* pointer, uint32_t time, uint32_t axis );
    void PointerAxisDiscrete( wl_pointer* pointer, uint32_t axis, int32_t discrete );
    void PointerAxisValue120( wl_pointer* pointer, uint32_t axis, int32_t value120 );
    void PointerAxisRelativeDirection( wl_pointer* pointer, uint32_t axis, uint32_t direction );
};

#endif
