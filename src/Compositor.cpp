#include <assert.h>

#include "Compositor.hpp"
#include "Display.hpp"
#include "Renderer.hpp"

extern "C" {
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_compositor.h>
};

Compositor::Compositor( const Display& dpy, const Renderer& renderer )
    : m_compositor( wlr_compositor_create( dpy, renderer ) )
{
    assert( m_compositor );
}
