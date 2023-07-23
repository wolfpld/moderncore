#include <fcntl.h>
#include <format>
#include <inttypes.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

extern "C" {
#include <libdisplay-info/info.h>
}

#include "DbusLoginPaths.hpp"
#include "DrmDevice.hpp"
#include "../../dbus/DbusSession.hpp"
#include "../../util/Logs.hpp"

static uint32_t GetRefreshRate( const drmModeModeInfo& mode )
{
    uint32_t refresh = mode.clock * 1000000ull / ( mode.htotal * mode.vtotal );
    if( mode.flags & DRM_MODE_FLAG_INTERLACE ) refresh *= 2;
    if( mode.flags & DRM_MODE_FLAG_DBLSCAN ) refresh /= 2;
    if( mode.vscan > 1 ) refresh /= mode.vscan;
    return refresh;
}

DrmDevice::DrmDevice( const char* devName, DbusSession& bus, const char* sessionPath )
    : m_bus( bus )
    , m_sessionPath( sessionPath )
    , m_dev( 0 )
    , m_fd( -1 )
    , m_res( nullptr )
{
    struct stat st;
    if( stat( devName, &st ) != 0 ) throw DeviceException( std::format( "Failed to stat DRM device: {}", strerror( errno ) ) );
    if( major( st.st_rdev ) != 226 ) throw DeviceException( "Not a DRM device" );

    auto devMsg = bus.Call( LoginService, sessionPath, LoginSessionIface, "TakeDevice", "uu", major( st.st_rdev ), minor( st.st_rdev ) );
    if( !devMsg ) throw DeviceException( std::format( "Failed to take device" ) );
    m_dev = st.st_rdev;

    int fd;
    int paused;
    if( !devMsg.Read( "hb", &fd, &paused ) ) throw DeviceException( "Failed to read device file descriptor" );

    if( !drmIsKMS( fd ) ) throw DeviceException( "Not a KMS device" );

    m_fd = fcntl( fd, F_DUPFD_CLOEXEC, 0 );
    if( m_fd < 0 ) throw DeviceException( std::format( "Failed to dup fd: {}", strerror( errno ) ) );

    auto ver = drmGetVersion( fd );
    mclog( LogLevel::Info, "DRM device %s: %s (%s)", devName, ver->name ? ver->name : "unknown", ver->desc ? ver->desc : "unknown" );
    drmFreeVersion( ver );

    m_res = drmModeGetResources( m_fd );
    if( !m_res ) throw DeviceException( "Failed to get resources" );

    drmGetCap( fd, DRM_CAP_DUMB_BUFFER, &m_caps.dumbBuffer );
    drmGetCap( fd, DRM_CAP_VBLANK_HIGH_CRTC, &m_caps.vblankHighCrtc );
    drmGetCap( fd, DRM_CAP_DUMB_PREFERRED_DEPTH, &m_caps.dumbPreferredDepth );
    drmGetCap( fd, DRM_CAP_DUMB_PREFER_SHADOW, &m_caps.dumbPreferShadow );
    drmGetCap( fd, DRM_CAP_PRIME, &m_caps.prime );
    drmGetCap( fd, DRM_CAP_TIMESTAMP_MONOTONIC, &m_caps.timestampMonotonic );
    drmGetCap( fd, DRM_CAP_ASYNC_PAGE_FLIP, &m_caps.asyncPageFlip );
    drmGetCap( fd, DRM_CAP_CURSOR_WIDTH, &m_caps.cursorWidth );
    drmGetCap( fd, DRM_CAP_CURSOR_HEIGHT, &m_caps.cursorHeight );
    drmGetCap( fd, DRM_CAP_ADDFB2_MODIFIERS, &m_caps.addFB2Modifiers );
    drmGetCap( fd, DRM_CAP_PAGE_FLIP_TARGET, &m_caps.pageFlipTarget );
    drmGetCap( fd, DRM_CAP_CRTC_IN_VBLANK_EVENT, &m_caps.crtcInVblankEvent );
    drmGetCap( fd, DRM_CAP_SYNCOBJ, &m_caps.syncObj );
    drmGetCap( fd, DRM_CAP_SYNCOBJ_TIMELINE, &m_caps.syncObjTimeline );

    mclog( LogLevel::Debug, "  dumb buffer: %" PRIu64, m_caps.dumbBuffer );
    mclog( LogLevel::Debug, "  vblank high crtc: %" PRIu64, m_caps.vblankHighCrtc );
    mclog( LogLevel::Debug, "  dumb preferred depth: %" PRIu64, m_caps.dumbPreferredDepth );
    mclog( LogLevel::Debug, "  dumb prefer shadow: %" PRIu64, m_caps.dumbPreferShadow );
    mclog( LogLevel::Debug, "  prime: %" PRIu64, m_caps.prime );
    mclog( LogLevel::Debug, "  timestamp monotonic: %" PRIu64, m_caps.timestampMonotonic );
    mclog( LogLevel::Debug, "  async page flip: %" PRIu64, m_caps.asyncPageFlip );
    mclog( LogLevel::Debug, "  cursor width: %" PRIu64, m_caps.cursorWidth );
    mclog( LogLevel::Debug, "  cursor height: %" PRIu64, m_caps.cursorHeight );
    mclog( LogLevel::Debug, "  addfb2 modifiers: %" PRIu64, m_caps.addFB2Modifiers );
    mclog( LogLevel::Debug, "  page flip target: %" PRIu64, m_caps.pageFlipTarget );
    mclog( LogLevel::Debug, "  crtc in vblank event: %" PRIu64, m_caps.crtcInVblankEvent );
    mclog( LogLevel::Debug, "  sync obj: %" PRIu64, m_caps.syncObj );
    mclog( LogLevel::Debug, "  sync obj timeline: %" PRIu64, m_caps.syncObjTimeline );

    mclog( LogLevel::Debug, "  %d connectors, %d encoders, %d crtcs, %d framebuffers", m_res->count_connectors, m_res->count_encoders, m_res->count_crtcs, m_res->count_fbs );
    mclog( LogLevel::Debug, "  min resolution: %dx%d, max resolution: %dx%d", m_res->min_width, m_res->min_height, m_res->max_width, m_res->max_height );

    for( int i=0; i<m_res->count_connectors; i++ )
    {
        auto conn = drmModeGetConnector( fd, m_res->connectors[i] );
        if( !conn )
        {
            mclog( LogLevel::Debug, "  Skipping connector %d: failed to get connector", i );
            continue;
        }

        auto cTypeName = drmModeGetConnectorTypeName( conn->connector_type );
        if( !cTypeName ) cTypeName = "unknown";
        auto cName = std::format( "{}-{}", cTypeName, conn->connector_type_id );

        if( conn->connection == DRM_MODE_DISCONNECTED )
        {
            mclog( LogLevel::Debug, "  Connector %s: disconnected", cName.c_str() );
        }
        else
        {
            std::string model = "unknown";
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
                                    model = di_info_get_model( info );
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

            mclog( LogLevel::Debug, "  Connector %s: %s, %dx%d mm", cName.c_str(), model.c_str(), conn->mmWidth, conn->mmHeight );

            for( int j=0; j<conn->count_modes; j++ )
            {
                const auto& mode = conn->modes[j];
                mclog( LogLevel::Debug, "    Mode %d: %dx%d @ %.3f Hz", j, mode.hdisplay, mode.vdisplay, GetRefreshRate( mode ) / 1000.f );
            }
        }

        drmModeFreeConnector( conn );
    }
}

DrmDevice::~DrmDevice()
{
    drmModeFreeResources( m_res );

    if( m_fd >= 0 ) close( m_fd );
    if( m_dev != 0 ) m_bus.Call( LoginService, m_sessionPath.c_str(), LoginSessionIface, "ReleaseDevice", "uu", major( m_dev ), minor( m_dev ) );
}
