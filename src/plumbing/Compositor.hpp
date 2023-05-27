#ifndef __COMPOSITOR_HPP__
#define __COMPOSITOR_HPP__

extern "C" {
    struct wlr_compositor;
};

class Display;
class Renderer;

class Compositor
{
public:
    Compositor( const Display& dpy, const Renderer& renderer );

    [[nodiscard]] operator wlr_compositor* () const { return m_compositor; }

private:
    wlr_compositor* m_compositor;
};

#endif
