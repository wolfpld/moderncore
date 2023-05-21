#ifndef __SEAT_HPP__
#define __SEAT_HPP__

extern "C"
{
    struct wlr_seat;
}

class Display;

class Seat
{
public:
    explicit Seat( const Display& dpy );
    ~Seat();

private:
    wlr_seat* m_seat;
};

#endif
