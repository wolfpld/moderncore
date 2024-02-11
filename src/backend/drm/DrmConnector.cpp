#include <algorithm>
#include <assert.h>
#include <bit>
#include <format>
#include <gbm.h>
#include <string.h>
#include <tracy/Tracy.hpp>
#include <xf86drmMode.h>

extern "C" {
#include <libdisplay-info/info.h>
}

#include "DrmDevice.hpp"
#include "DrmConnector.hpp"
#include "DrmCrtc.hpp"
#include "util/Logs.hpp"

static uint32_t GetRefreshRate( const drmModeModeInfo& mode )
{
    uint32_t refresh = mode.clock * 1000000ull / ( mode.htotal * mode.vtotal );
    if( mode.flags & DRM_MODE_FLAG_INTERLACE ) refresh *= 2;
    if( mode.flags & DRM_MODE_FLAG_DBLSCAN ) refresh /= 2;
    if( mode.vscan > 1 ) refresh /= mode.vscan;
    return refresh;
}

DrmConnector::DrmConnector( DrmDevice& device, uint32_t id, const drmModeRes* res )
    : m_id( id )
    , m_bo( nullptr )
    , m_monitor( "unknown" )
    , m_device( device )
{
    ZoneScoped;

    auto conn = drmModeGetConnector( device.Descriptor(), id );
    if( !conn ) throw ConnectorException( "Failed to get connector" );
    assert( id == conn->connector_id );

    auto cTypeName = drmModeGetConnectorTypeName( conn->connector_type );
    if( !cTypeName ) cTypeName = "unknown";
    m_name = std::format( "{}-{}", cTypeName, conn->connector_type_id );

    m_connected = conn->connection != DRM_MODE_DISCONNECTED;

    auto crtcs = drmModeConnectorGetPossibleCrtcs( device.Descriptor(), conn );
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
        mclog( LogLevel::Info, "  Connector %s: disconnected", m_name.c_str() );
    }
    else
    {
        auto props = drmModeObjectGetProperties( device.Descriptor(), conn->connector_id, DRM_MODE_OBJECT_CONNECTOR );
        if( props )
        {
            for( int j=0; j<props->count_props; j++ )
            {
                auto prop = drmModeGetProperty( device.Descriptor(), props->props[j] );
                if( prop )
                {
                    if( strcmp( prop->name, "EDID" ) == 0 )
                    {
                        auto blob = drmModeGetPropertyBlob( device.Descriptor(), props->prop_values[j] );
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
            m_modes.emplace_back( mode );
            mclog( LogLevel::Debug, "    Mode %d: %dx%d @ %.3f Hz", j, mode.hdisplay, mode.vdisplay, GetRefreshRate( mode ) / 1000.f );
        }
    }

    drmModeFreeConnector( conn );
}

DrmConnector::~DrmConnector()
{
    if( m_bo )
    {
        drmModeRmFB( m_device.Descriptor(), m_fbId );
        gbm_bo_destroy( m_bo );
    }
}

bool DrmConnector::SetMode( const drmModeModeInfo& mode )
{
    ZoneScoped;
    assert( m_connected );

    auto& crtcs = m_device.GetCrtcs();
    auto it = std::find_if( crtcs.begin(), crtcs.end(), []( const auto& c ) { return !c->IsUsed(); } );
    if( it == crtcs.end() ) return false;

    auto& crtc = *it;

    // todo
    assert( !m_bo );

    m_bo = gbm_bo_create( m_device, mode.hdisplay, mode.vdisplay, GBM_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING );
    if( !m_bo ) return false;

    if( drmModeAddFB( m_device.Descriptor(), mode.hdisplay, mode.vdisplay, 24, 32, gbm_bo_get_stride( m_bo ), gbm_bo_get_handle( m_bo ).u32, &m_fbId ) != 0 )
    {
        gbm_bo_destroy( m_bo );
        m_bo = nullptr;
        return false;
    }

    auto _mode = mode;
    if( drmModeSetCrtc( m_device.Descriptor(), crtc->GetId(), m_fbId, 0, 0, &m_id, 1, &_mode ) != 0 )
    {
        drmModeRmFB( m_device.Descriptor(), m_fbId );
        gbm_bo_destroy( m_bo );
        m_bo = nullptr;
        return false;
    }

    crtc->Enable();

    return true;
}

const drmModeModeInfo& DrmConnector::GetBestDisplayMode() const
{
    assert( m_connected );
    assert( !m_modes.empty() );

    uint32_t best = 0;
    uint32_t bestRes = m_modes[0].hdisplay * m_modes[0].vdisplay;
    uint32_t bestRefresh = GetRefreshRate( m_modes[0] );
    for( uint32_t i=1; i<m_modes.size(); i++ )
    {
        const auto refresh = GetRefreshRate( m_modes[i] );
        const uint32_t res = m_modes[i].hdisplay * m_modes[i].vdisplay;
        if( res > bestRes )
        {
            best = i;
            bestRes = res;
            bestRefresh = refresh;
        }
        else if( refresh > bestRefresh )
        {
            best = i;
            bestRefresh = refresh;
        }
    }

    return m_modes[best];
}
