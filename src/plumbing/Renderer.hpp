#ifndef __WLR_RENDERER_HPP__
#define __WLR_RENDERER_HPP__

extern "C" {
    struct wlr_renderer;
};

class BackendWlr;
class Display;

class Renderer
{
public:
    Renderer( const BackendWlr& backend, const Display& dpy );
    ~Renderer();

    [[nodiscard]] operator wlr_renderer* () const { return m_renderer; }

private:
    wlr_renderer* m_renderer;
};

#endif
