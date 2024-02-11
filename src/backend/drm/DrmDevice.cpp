#include <fcntl.h>
#include <format>
#include <gbm.h>
#include <inttypes.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <tracy/Tracy.hpp>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "DbusLoginPaths.hpp"
#include "DrmCrtc.hpp"
#include "DrmConnector.hpp"
#include "DrmDevice.hpp"
#include "dbus/DbusSession.hpp"
#include "util/Logs.hpp"
#include "util/Panic.hpp"

constexpr int DriMajor = 226;

DrmDevice::DrmDevice( const char* devName, DbusSession& bus, const char* sessionPath )
    : m_sessionPath( sessionPath )
    , m_dev( 0 )
    , m_fd( -1 )
    , m_res( nullptr )
{
    ZoneScoped;

    struct stat st;
    if( stat( devName, &st ) != 0 ) throw DeviceException( std::format( "Failed to stat DRM device: {}", strerror( errno ) ) );
    if( major( st.st_rdev ) != DriMajor ) throw DeviceException( "Not a DRM device" );

    auto devMsg = bus.Call( LoginService, sessionPath, LoginSessionIface, "TakeDevice", "uu", major( st.st_rdev ), minor( st.st_rdev ) );
    if( !devMsg ) throw DeviceException( std::format( "Failed to take device" ) );
    m_dev = st.st_rdev;

    int fd;
    int paused;
    if( !devMsg.Read( "hb", &fd, &paused ) ) throw DeviceException( "Failed to read device file descriptor" );

    if( !drmIsKMS( fd ) ) throw DeviceException( "Not a KMS device" );

    m_fd = fcntl( fd, F_DUPFD_CLOEXEC, 0 );
    if( m_fd < 0 ) throw DeviceException( std::format( "Failed to dup fd: {}", strerror( errno ) ) );

    if( drmSetClientCap( m_fd, DRM_CLIENT_CAP_ATOMIC, 1 ) != 0 ) throw DeviceException( "Failed to set atomic cap" );

    m_gbm = gbm_create_device( m_fd );
    if( !m_gbm ) throw DeviceException( "Failed to create GBM device" );

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

    mclog( LogLevel::Debug, "  %d connectors, %d crtcs, %d framebuffers", m_res->count_connectors, m_res->count_crtcs, m_res->count_fbs );
    mclog( LogLevel::Debug, "  min resolution: %dx%d, max resolution: %dx%d", m_res->min_width, m_res->min_height, m_res->max_width, m_res->max_height );

    for( int i=0; i<m_res->count_crtcs; i++ )
    {
        try
        {
            m_crtcs.emplace_back( std::make_unique<DrmCrtc>( m_fd, m_res->crtcs[i] ) );
        }
        catch( DrmCrtc::CrtcException& e )
        {
            mclog( LogLevel::Warning, "  Skipping CRTC %d: %s", i, e.what() );
        }
    }

    for( int i=0; i<m_res->count_connectors; i++ )
    {
        try
        {
            m_connectors.emplace_back( std::make_unique<DrmConnector>( *this, m_res->connectors[i], m_res ) );
        }
        catch( DrmConnector::ConnectorException& e )
        {
            mclog( LogLevel::Warning, "  Skipping connector %d: %s", i, e.what() );
        }
    }

    bool found = false;
    for( auto& conn : m_connectors )
    {
        if( !conn->IsConnected() ) continue;

        auto& mode = conn->GetBestDisplayMode();
        mclog( LogLevel::Info, "  Setting connector %s to %dx%d @ %d Hz", conn->GetName().c_str(), mode.hdisplay, mode.vdisplay, mode.vrefresh );
        CheckPanic( conn->SetMode( mode ), "Failed to set mode" );
        found = true;
        break;
    }
    if( !found ) mclog( LogLevel::Warning, "  No connected displays found" );

    for( auto& crtc : m_crtcs )
    {
        if( !crtc->IsUsed() ) crtc->Disable();
    }
}

DrmDevice::~DrmDevice()
{
    m_connectors.clear();
    m_crtcs.clear();

    drmModeFreeResources( m_res );

    gbm_device_destroy( m_gbm );

    if( m_fd >= 0 ) close( m_fd );
    if( m_dev != 0 )
    {
        DbusSession bus;
        bus.Call( LoginService, m_sessionPath.c_str(), LoginSessionIface, "ReleaseDevice", "uu", major( m_dev ), minor( m_dev ) );
    }
}
