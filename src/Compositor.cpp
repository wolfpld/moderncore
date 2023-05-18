#include "Compositor.hpp"
#include "Display.hpp"
#include "Panic.hpp"
#include "Renderer.hpp"

extern "C" {
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_compositor.h>
};

Compositor::Compositor( const Display& dpy, const Renderer& renderer )
    : m_compositor( wlr_compositor_create( dpy, renderer ) )
{
    CheckPanic( m_compositor, "Failed to create wlr_compositor!" );
}
