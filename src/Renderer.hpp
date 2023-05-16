#ifndef __RENDERER_HPP__
#define __RENDERER_HPP__

extern "C" {
    struct wlr_renderer;
};

class Backend;
class Display;

class Renderer
{
public:
    Renderer( const Backend& backend, const Display& dpy );
    ~Renderer();

private:
    struct wlr_renderer* m_renderer;
};

#endif
