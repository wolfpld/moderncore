#include "Backend.hpp"
#include "Display.hpp"
#include "../util/Panic.hpp"

extern "C" {
#define WLR_USE_UNSTABLE
#include <wlr/backend.h>
};

Backend::Backend( const Display& dpy )
    : m_backend( wlr_backend_autocreate( dpy ) )
{
    CheckPanic( m_backend, "Failed to create wlr_backend!" );
}

Backend::~Backend()
{
    wlr_backend_destroy( m_backend );
}

void Backend::Start()
{
    CheckPanic( wlr_backend_start( m_backend ), "Unable to start wlr_backend!" );
}
