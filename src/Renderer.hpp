#ifndef __RENDERER_HPP__
#define __RENDERER_HPP__

extern "C" {
    struct wlr_renderer;
};

class Backend;

class Renderer
{
public:
    explicit Renderer( const Backend& backend );
    ~Renderer();

private:
    struct wlr_renderer* m_renderer;
};

#endif
