#ifndef __BACKEND_HPP__
#define __BACKEND_HPP__

extern "C" {
    struct wlr_backend;
};

class Display;

class Backend
{
public:
    Backend( const Display& dpy );
    ~Backend();

private:
    struct wlr_backend* m_backend;
};

#endif
