#include <assert.h>
#include <xf86drmMode.h>

#include "DrmCrtc.hpp"
#include "util/Logs.hpp"

DrmCrtc::DrmCrtc( int fd, uint32_t id, uint32_t mask )
    : m_fd( fd )
    , m_id( id )
    , m_mask( mask )
    , m_inUse( false )
{
    auto crtc = drmModeGetCrtc( fd, id );
    if( !crtc ) throw CrtcException( "Failed to get CRTC" );
    assert( id == crtc->crtc_id );
    m_bufferId = crtc->buffer_id;
    drmModeFreeCrtc( crtc );
}

DrmCrtc::~DrmCrtc()
{
}

void DrmCrtc::Enable()
{
    m_inUse = true;
}

void DrmCrtc::Disable()
{
    m_inUse = false;

    if( m_bufferId != 0 )
    {
        mclog( LogLevel::Debug, "CRTC %u: disabling", m_id );
        drmModeSetCrtc( m_fd, m_id, 0, 0, 0, nullptr, 0, nullptr );
        m_bufferId = 0;
    }
}
