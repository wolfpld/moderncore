#ifndef __SEAT_HPP__
#define __SEAT_HPP__

#include <memory>
#include <vector>

#include "Cursor.hpp"
#include "Listener.hpp"

extern "C"
{
    struct wlr_seat;
    struct wlr_input_device;
    struct wlr_seat_pointer_request_set_cursor_event;
    struct wlr_seat_request_set_selection_event;
}

class Backend;
class Display;
class Output;
class Pointer;

class Seat
{
public:
    Seat( const Display& dpy, const Backend& backend, const Output& output );
    ~Seat();

    void Remove( const Pointer* pointer );

    [[nodiscard]] const Cursor& GetCursor() const { return m_cursor; }
    [[nodiscard]] operator wlr_seat* () const { return m_seat; }

private:
    void NewInput( wlr_input_device* dev );
    void ReqCursor( wlr_seat_pointer_request_set_cursor_event* ev );
    void ReqSetSelection( wlr_seat_request_set_selection_event* ev );

    void NewPointer( wlr_input_device* dev );

    Cursor m_cursor;

    wlr_seat* m_seat;
    Listener<wlr_input_device> m_newInput;
    Listener<wlr_seat_pointer_request_set_cursor_event> m_reqCursor;
    Listener<wlr_seat_request_set_selection_event> m_reqSetSelection;

    std::vector<std::unique_ptr<Pointer>> m_pointers;
};

#endif
