#include "Backend.hpp"
#include "Display.hpp"
#include "../util/Panic.hpp"

extern "C" {
#define WLR_USE_UNSTABLE
#include <wlr/backend.h>
};

BackendWlr::BackendWlr( const Display& dpy )
    : m_backend( wlr_backend_autocreate( dpy ) )
{
    CheckPanic( m_backend, "Failed to create wlr_backend!" );
}

BackendWlr::~BackendWlr()
{
    wlr_backend_destroy( m_backend );
}

void BackendWlr::Start()
{
    CheckPanic( wlr_backend_start( m_backend ), "Unable to start wlr_backend!" );
}
