#include <assert.h>
#include <xf86drmMode.h>

#include "DrmCrtc.hpp"

DrmCrtc::DrmCrtc( int fd, uint32_t id )
    : m_id( id )
{
    auto crtc = drmModeGetCrtc( fd, id );
    if( !crtc ) throw CrtcException( "Failed to get CRTC" );
    assert( id == crtc->crtc_id );

    if( crtc->buffer_id )
    {
        drmModeSetCrtc( fd, id, 0, 0, 0, nullptr, 0, nullptr );
    }

    drmModeFreeCrtc( crtc );
}

DrmCrtc::~DrmCrtc()
{
}
