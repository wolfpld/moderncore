#include "Display.hpp"
#include "Panic.hpp"
#include "Subcompositor.hpp"

extern "C" {
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_subcompositor.h>
};

Subcompositor::Subcompositor( const Display& dpy )
    : m_subcompositor( wlr_subcompositor_create( dpy ) )
{
    CheckPanic( m_subcompositor, "Failed to create wlr_subcompositor!" );
}
