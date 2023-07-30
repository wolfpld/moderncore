#include <assert.h>
#include <xf86drmMode.h>

#include "DrmCrtc.hpp"
#include "../../util/Logs.hpp"

DrmCrtc::DrmCrtc( int fd, uint32_t id )
    : m_fd( fd )
    , m_id( id )
    , m_inUse( false )
{
    auto crtc = drmModeGetCrtc( fd, id );
    if( !crtc ) throw CrtcException( "Failed to get CRTC" );
    assert( id == crtc->crtc_id );
    m_isAttached = crtc->buffer_id != 0;
    drmModeFreeCrtc( crtc );
}

DrmCrtc::~DrmCrtc()
{
}

void DrmCrtc::Enable()
{
    m_inUse = true;
    m_isAttached = true;
}

void DrmCrtc::Disable()
{
    m_inUse = false;

    if( m_isAttached )
    {
        mclog( LogLevel::Debug, "CRTC %u: disabling", m_id );
        drmModeSetCrtc( m_fd, m_id, 0, 0, 0, nullptr, 0, nullptr );
        m_isAttached = false;
    }
}
