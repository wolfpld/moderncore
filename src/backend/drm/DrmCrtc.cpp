#include <tracy/Tracy.hpp>
#include <xf86drmMode.h>

#include "DrmCrtc.hpp"
#include "util/Logs.hpp"
#include "util/Panic.hpp"

DrmCrtc::DrmCrtc( int fd, uint32_t id, uint32_t mask )
    : m_fd( fd )
    , m_id( id )
    , m_mask( mask )
{
    ZoneScoped;

    auto crtc = drmModeGetCrtc( fd, id );
    if( !crtc ) throw CrtcException( "Failed to get CRTC" );
    CheckPanic( id == crtc->crtc_id, "CRTC ID mismatch" );
    m_bufferId = crtc->buffer_id;
    drmModeFreeCrtc( crtc );
}

DrmCrtc::~DrmCrtc()
{
}

void DrmCrtc::Enable()
{
}

void DrmCrtc::Disable()
{
    ZoneScoped;
    CheckPanic( m_bufferId != 0, "CRTC %u: already disabled", m_id );

    mclog( LogLevel::Debug, "CRTC %u: disabling", m_id );
    drmModeSetCrtc( m_fd, m_id, 0, 0, 0, nullptr, 0, nullptr );
    m_bufferId = 0;
}
