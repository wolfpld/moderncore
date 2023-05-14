#include <assert.h>

#include "Backend.hpp"
#include "Renderer.hpp"

extern "C" {
    /* wlr_renderer.h header uses C99 features not recognized by a C++ compiler */
    struct wlr_renderer *wlr_renderer_autocreate(struct wlr_backend *backend);
    void wlr_renderer_destroy(struct wlr_renderer *renderer);
};

Renderer::Renderer( const Backend& backend )
    : m_renderer( wlr_renderer_autocreate( backend ) )
{
    assert( m_renderer );
}

Renderer::~Renderer()
{
    wlr_renderer_destroy( m_renderer );
}
