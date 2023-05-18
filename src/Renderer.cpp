#include "Backend.hpp"
#include "Display.hpp"
#include "Panic.hpp"
#include "Renderer.hpp"

extern "C" {
    /* wlr_renderer.h header uses C99 features not recognized by a C++ compiler */
    struct wlr_renderer *wlr_renderer_autocreate(struct wlr_backend *backend);
    void wlr_renderer_destroy(struct wlr_renderer *renderer);
    bool wlr_renderer_init_wl_display(struct wlr_renderer *r, struct wl_display *wl_display);
};

Renderer::Renderer( const Backend& backend, const Display& dpy )
    : m_renderer( wlr_renderer_autocreate( backend ) )
{
    CheckPanic( m_renderer, "Failed to create wlr_renderer!" );
    wlr_renderer_init_wl_display( m_renderer, dpy );
}

Renderer::~Renderer()
{
    wlr_renderer_destroy( m_renderer );
}
