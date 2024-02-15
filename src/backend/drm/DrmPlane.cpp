#include <drm_fourcc.h>
#include <xf86drmMode.h>

#include "DrmPlane.hpp"
#include "DrmProperties.hpp"

DrmPlane::DrmPlane( int fd, uint32_t id )
    : m_plane( drmModeGetPlane( fd, id ) )
{
    if( !m_plane ) throw PlaneException( "Failed to get plane" );

    DrmProperties props( fd, id, DRM_MODE_OBJECT_PLANE );
    if( !props ) throw PlaneException( "Failed to get plane properties" );

    auto formatsProp = props["IN_FORMATS"];
    if( !formatsProp ) throw PlaneException( "Failed to get plane formats" );

    auto blob = formatsProp.Blob();
    if( !blob ) throw PlaneException( "Failed to get plane formats blob" );

    auto data = (drm_format_modifier_blob*)blob->data;
    auto formats = (uint32_t*)( ((char*)data) + data->formats_offset );
    auto modifiers = (drm_format_modifier*)( ((char*)data) + data->modifiers_offset );

    for( uint32_t f=0; f<data->count_formats; f++ )
    {
        if( formats[f] != DRM_FORMAT_XRGB8888 ) continue;

        for( uint32_t m=0; m<data->count_modifiers; m++ )
        {
            auto& mod = modifiers[m];
            if( f < mod.offset || f > mod.offset + 63 ) continue;
            if( !( mod.formats & ( 1 << ( f - mod.offset ) ) ) ) continue;

            m_modifiers.emplace_back( mod.modifier );
        }
    }
}

DrmPlane::~DrmPlane()
{
    drmModeFreePlane( m_plane );
}
