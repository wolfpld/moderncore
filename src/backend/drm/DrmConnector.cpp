#include <assert.h>
#include <bit>
#include <format>
#include <string.h>
#include <xf86drmMode.h>

extern "C" {
#include <libdisplay-info/info.h>
}

#include "DrmConnector.hpp"
#include "../../util/Logs.hpp"

static uint32_t GetRefreshRate( const drmModeModeInfo& mode )
{
    uint32_t refresh = mode.clock * 1000000ull / ( mode.htotal * mode.vtotal );
    if( mode.flags & DRM_MODE_FLAG_INTERLACE ) refresh *= 2;
    if( mode.flags & DRM_MODE_FLAG_DBLSCAN ) refresh /= 2;
    if( mode.vscan > 1 ) refresh /= mode.vscan;
    return refresh;
}

DrmConnector::DrmConnector( int fd, uint32_t id, const drmModeRes* res )
    : m_id( id )
    , m_monitor( "unknown" )
{
    auto conn = drmModeGetConnector( fd, id );
    if( !conn ) throw ConnectorException( "Failed to get connector" );
    assert( id == conn->connector_id );

    auto cTypeName = drmModeGetConnectorTypeName( conn->connector_type );
    if( !cTypeName ) cTypeName = "unknown";
    m_name = std::format( "{}-{}", cTypeName, conn->connector_type_id );

    m_connected = conn->connection != DRM_MODE_DISCONNECTED;

    auto crtcs = drmModeConnectorGetPossibleCrtcs( fd, conn );
    if( crtcs == 0 )
    {
        drmModeFreeConnector( conn );
        throw ConnectorException( "Connector has no CRTCs" );
    }

    while( crtcs != 0 )
    {
        auto bit = std::countr_zero( crtcs );
        crtcs &= ~( 1 << bit );
        assert( bit < res->count_crtcs );
        m_crtcs.push_back( bit );
    }

    if( !m_connected )
    {
        mclog( LogLevel::Debug, "  Connector %s: disconnected", m_name.c_str() );
    }
    else
    {
        auto props = drmModeObjectGetProperties( fd, conn->connector_id, DRM_MODE_OBJECT_CONNECTOR );
        if( props )
        {
            for( int j=0; j<props->count_props; j++ )
            {
                auto prop = drmModeGetProperty( fd, props->props[j] );
                if( prop )
                {
                    if( strcmp( prop->name, "EDID" ) == 0 )
                    {
                        auto blob = drmModeGetPropertyBlob( fd, props->prop_values[j] );
                        if( blob )
                        {
                            auto info = di_info_parse_edid( blob->data, blob->length );
                            if( info )
                            {
                                m_monitor = di_info_get_model( info );
                                di_info_destroy( info );
                            }
                            drmModeFreePropertyBlob( blob );
                        }
                    }
                    drmModeFreeProperty( prop );
                }
            }
        }
        drmModeFreeObjectProperties( props );

        mclog( LogLevel::Debug, "  Connector %s: %s, %dx%d mm", m_name.c_str(), m_monitor.c_str(), conn->mmWidth, conn->mmHeight );

        for( int j=0; j<conn->count_modes; j++ )
        {
            const auto& mode = conn->modes[j];
            mclog( LogLevel::Debug, "    Mode %d: %dx%d @ %.3f Hz", j, mode.hdisplay, mode.vdisplay, GetRefreshRate( mode ) / 1000.f );
        }
    }

    drmModeFreeConnector( conn );
}

DrmConnector::~DrmConnector()
{
}
