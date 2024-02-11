#include <xf86drmMode.h>

#include "DrmPlane.hpp"

DrmPlane::DrmPlane( int fd, uint32_t id )
    : m_plane( drmModeGetPlane( fd, id ) )
{
    if( !m_plane ) throw PlaneException( "Failed to get plane" );
}

DrmPlane::~DrmPlane()
{
    drmModeFreePlane( m_plane );
}
