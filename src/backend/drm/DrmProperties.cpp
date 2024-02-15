#include <string.h>

#include "DrmProperties.hpp"
#include "util/Logs.hpp"

/* DrmProperty */

DrmProperty::DrmProperty( int fd, drmModePropertyPtr prop, uint64_t value )
    : m_fd( fd )
    , m_value( value )
    , m_prop( prop )
    , m_blob( nullptr )
{
}

DrmProperty::~DrmProperty()
{
    if( m_blob ) drmModeFreePropertyBlob( m_blob );
    if( m_prop ) drmModeFreeProperty( m_prop );
}

drmModePropertyBlobPtr DrmProperty::Blob()
{
    if( m_blob ) return m_blob;
    if( m_prop->flags & DRM_MODE_PROP_BLOB )
    {
        m_blob = drmModeGetPropertyBlob( m_fd, m_value );
        return m_blob;
    }
    return nullptr;
}

/* DrmProperties */

DrmProperties::DrmProperties( int fd, uint32_t id, uint32_t type )
    : m_fd( fd )
    , m_props( drmModeObjectGetProperties( fd, id, type ) )
{
}

DrmProperties::~DrmProperties()
{
    drmModeFreeObjectProperties( m_props );
}

void DrmProperties::List() const
{
    for( int i=0; i<m_props->count_props; i++ )
    {
        auto prop = drmModeGetProperty( m_fd, m_props->props[i] );
        if( !prop ) continue;
        mclog( LogLevel::Info, "  %s: %llu", prop->name, m_props->prop_values[i] );
        drmModeFreeProperty( prop );
    }
}

DrmProperty DrmProperties::operator[]( const char* name ) const
{
    for( int i=0; i<m_props->count_props; i++ )
    {
        auto prop = drmModeGetProperty( m_fd, m_props->props[i] );
        if( !prop ) continue;
        if( strcmp( prop->name, name ) == 0 ) return DrmProperty( m_fd, prop, m_props->prop_values[i] );
        drmModeFreeProperty( prop );
    }
    return {};
}
