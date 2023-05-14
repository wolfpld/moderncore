#include <assert.h>

#include "Backend.hpp"
#include "Display.hpp"

extern "C" {
#define WLR_USE_UNSTABLE
#include <wlr/backend.h>
};

Backend::Backend( const Display& dpy )
    : m_backend( wlr_backend_autocreate( dpy ) )
{
    assert( m_backend );
}

Backend::~Backend()
{
    wlr_backend_destroy( m_backend );
}
