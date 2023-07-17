#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <systemd/sd-device.h>
#include <systemd/sd-login.h>
#include <unistd.h>
#include <xf86drm.h>

#include "BackendDrm.hpp"
#include "../../dbus/DbusSession.hpp"
#include "../../util/Panic.hpp"

#define DbusCallback( func ) [this] ( DbusMessage msg ) { return func( std::move( msg ) ); }

constexpr const char* LoginService = "org.freedesktop.login1";
constexpr const char* LoginPath = "/org/freedesktop/login1"; 
constexpr const char* LoginManagerIface = "org.freedesktop.login1.Manager";
constexpr const char* LoginSessionIface = "org.freedesktop.login1.Session";
constexpr const char* DbusPropertiesIface = "org.freedesktop.DBus.Properties";

static uint32_t GetRefreshRate( const drmModeModeInfo& mode )
{
    uint32_t refresh = mode.clock * 1000000ull / ( mode.htotal * mode.vtotal );
    if( mode.flags & DRM_MODE_FLAG_INTERLACE ) refresh *= 2;
    if( mode.flags & DRM_MODE_FLAG_DBLSCAN ) refresh /= 2;
    if( mode.vscan > 1 ) refresh /= mode.vscan;
    return refresh;
}

DrmDevice::~DrmDevice()
{
    drmModeFreeResources( res );
}

BackendDrm::BackendDrm( VkInstance vkInstance, DbusSession& bus )
    : m_session( nullptr )
    , m_seat( nullptr )
{
    CheckPanic( bus, "Cannot continue without a valid Dbus connection." );

    if( const auto res = sd_pid_get_session( 0, &m_session ); res < 0 ) CheckPanic( false, "Failed to get session: %s", strerror( -res ) );
    if( const auto res = sd_session_get_seat( m_session, &m_seat ); res < 0 ) CheckPanic( false, "Failed to get seat for session %s: %s", m_session, strerror( -res ) );
    if( sd_seat_can_graphical( m_seat ) <= 0 ) CheckPanic( false, "Seat %s is not graphical", m_seat );

    mclog( LogLevel::Debug, "Login session: %s, seat: %s", m_session, m_seat );

    {
        auto sessionMsg = bus.Call( LoginService, LoginPath, LoginManagerIface, "GetSession", "s", m_session );
        CheckPanic( sessionMsg, "Failed to get session object" );
        const char* sessionPath;
        CheckPanic( sessionMsg.Read( "o", &sessionPath ), "Failed to read session object" );
        m_sessionPath = sessionPath;
    }

    {
        auto seatMsg = bus.Call( LoginService, LoginPath, LoginManagerIface, "GetSeat", "s", m_seat );
        CheckPanic( seatMsg, "Failed to get seat object" );
        const char* seatPath;
        CheckPanic( seatMsg.Read( "o", &seatPath ), "Failed to read seat object" );
        m_seatPath = seatPath;
    }

    mclog( LogLevel::Debug, "Session object: %s", m_sessionPath.c_str() );
    mclog( LogLevel::Debug, "Seat object: %s", m_seatPath.c_str() );

    bus.MatchSignal( LoginService, m_sessionPath.c_str(), LoginSessionIface, "PauseDevice", DbusCallback( PauseDevice ) );
    bus.MatchSignal( LoginService, m_sessionPath.c_str(), LoginSessionIface, "ResumeDevice", DbusCallback( ResumeDevice ) );
    bus.MatchSignal( LoginService, m_sessionPath.c_str(), DbusPropertiesIface, "PropertiesChanged", DbusCallback( PropertiesChanged ) );

    CheckPanic( bus.Call( LoginService, m_sessionPath.c_str(), LoginSessionIface, "Activate" ), "Failed to activate session" );
    CheckPanic( bus.Call( LoginService, m_sessionPath.c_str(), LoginSessionIface, "TakeControl", "b", false ), "Failed to take control of session" );

    sd_device_enumerator* e = nullptr;
    CheckPanic( sd_device_enumerator_new( &e ) >= 0, "Failed to create device enumerator" );
    CheckPanic( sd_device_enumerator_add_match_subsystem( e, "drm", true ) >= 0, "Failed to mach drm subsystem" );
    CheckPanic( sd_device_enumerator_add_match_sysname( e, "card[0-9]*" ) >= 0, "Failed to match drm device" );

    for( sd_device* dev = sd_device_enumerator_get_device_first( e ); dev; dev = sd_device_enumerator_get_device_next( e ) )
    {
        const char* seat = nullptr;
        sd_device_get_property_value( dev, "ID_SEAT", &seat );
        if( !seat ) seat = "seat0";

        const char* sysPath;
        sd_device_get_syspath( dev, &sysPath );

        if( strcmp( seat, m_seat ) != 0 )
        {
            mclog( LogLevel::Debug, "Skipping DRM device %s on seat %s", sysPath, seat );
            continue;
        }

        bool isBoot = false;
        sd_device* pci = nullptr;
        sd_device_get_parent_with_subsystem_devtype( dev, "pci", nullptr, &pci );
        if( pci )
        {
            const char* bootVga;
            sd_device_get_sysattr_value( pci, "boot_vga", &bootVga );
            if( bootVga && strcmp( bootVga, "1" ) == 0 )
            {
                isBoot = true;
            }
        }

        const char* devName = nullptr;
        sd_device_get_devname( dev, &devName );
        if( !devName )
        {
            mclog( LogLevel::Debug, "Skipping DRM device %s: no devname", sysPath );
            continue;
        }

        struct stat st;
        if( stat( devName, &st ) != 0 )
        {
            mclog( LogLevel::Debug, "Skipping DRM device %s: stat failed: %s", sysPath, strerror( errno ) );
            continue;
        }

        if( major( st.st_rdev ) != 226 )
        {
            mclog( LogLevel::Debug, "Skipping DRM device %s: not a DRM device", sysPath );
            continue;
        }

        auto devMsg = bus.Call( LoginService, m_sessionPath.c_str(), LoginSessionIface, "TakeDevice", "uu", major( st.st_rdev ), minor( st.st_rdev ) );
        if( !devMsg )
        {
            mclog( LogLevel::Debug, "Skipping DRM device %s: failed to take device", sysPath );
            continue;
        }

        int fd;
        int paused;
        if( !devMsg.Read( "hb", &fd, &paused ) )
        {
            mclog( LogLevel::Debug, "Skipping DRM device %s: failed to read device fd", sysPath );
            continue;
        }

        if( !drmIsKMS( fd ) )
        {
            bus.Call( LoginService, m_sessionPath.c_str(), LoginSessionIface, "ReleaseDevice", "uu", major( st.st_rdev ), minor( st.st_rdev ) );
            mclog( LogLevel::Debug, "Skipping DRM device %s: not a KMS device", sysPath );
            continue;
        }

        fd = fcntl( fd, F_DUPFD_CLOEXEC, 0 );
        if( fd < 0 )
        {
            mclog( LogLevel::Debug, "Skipping DRM device %s: failed to dup fd: %s", sysPath, strerror( errno ) );
            continue;
        }

        const auto name = drmGetDeviceNameFromFd2( fd );
        auto ver = drmGetVersion( fd );

        mclog( LogLevel::Info, "DRM device %s (%s) on seat %s%s, driver: %s (%s)", sysPath, name, seat, isBoot ? " (boot vga)" : "", ver ? ver->name : "unknown", ver ? ver->desc : "unknown" );

        drmFreeVersion( ver );
        free( name );

        DrmDevice drmDev = {};
        drmDev.fd = fd;
        drmDev.res = drmModeGetResources( fd );

        if( !drmDev.res )
        {
            mclog( LogLevel::Debug, "Skipping DRM device %s: failed to get resources", sysPath );
            continue;
        }

        drmGetCap( fd, DRM_CAP_DUMB_BUFFER, &drmDev.caps.dumbBuffer );
        drmGetCap( fd, DRM_CAP_VBLANK_HIGH_CRTC, &drmDev.caps.vblankHighCrtc );
        drmGetCap( fd, DRM_CAP_DUMB_PREFERRED_DEPTH, &drmDev.caps.dumbPreferredDepth );
        drmGetCap( fd, DRM_CAP_DUMB_PREFER_SHADOW, &drmDev.caps.dumbPreferShadow );
        drmGetCap( fd, DRM_CAP_PRIME, &drmDev.caps.prime );
        drmGetCap( fd, DRM_CAP_TIMESTAMP_MONOTONIC, &drmDev.caps.timestampMonotonic );
        drmGetCap( fd, DRM_CAP_ASYNC_PAGE_FLIP, &drmDev.caps.asyncPageFlip );
        drmGetCap( fd, DRM_CAP_CURSOR_WIDTH, &drmDev.caps.cursorWidth );
        drmGetCap( fd, DRM_CAP_CURSOR_HEIGHT, &drmDev.caps.cursorHeight );
        drmGetCap( fd, DRM_CAP_ADDFB2_MODIFIERS, &drmDev.caps.addFB2Modifiers );
        drmGetCap( fd, DRM_CAP_PAGE_FLIP_TARGET, &drmDev.caps.pageFlipTarget );
        drmGetCap( fd, DRM_CAP_CRTC_IN_VBLANK_EVENT, &drmDev.caps.crtcInVblankEvent );
        drmGetCap( fd, DRM_CAP_SYNCOBJ, &drmDev.caps.syncObj );
        drmGetCap( fd, DRM_CAP_SYNCOBJ_TIMELINE, &drmDev.caps.syncObjTimeline );

        mclog( LogLevel::Debug, "  dumb buffer: %" PRIu64, drmDev.caps.dumbBuffer );
        mclog( LogLevel::Debug, "  vblank high crtc: %" PRIu64, drmDev.caps.vblankHighCrtc );
        mclog( LogLevel::Debug, "  dumb preferred depth: %" PRIu64, drmDev.caps.dumbPreferredDepth );
        mclog( LogLevel::Debug, "  dumb prefer shadow: %" PRIu64, drmDev.caps.dumbPreferShadow );
        mclog( LogLevel::Debug, "  prime: %" PRIu64, drmDev.caps.prime );
        mclog( LogLevel::Debug, "  timestamp monotonic: %" PRIu64, drmDev.caps.timestampMonotonic );
        mclog( LogLevel::Debug, "  async page flip: %" PRIu64, drmDev.caps.asyncPageFlip );
        mclog( LogLevel::Debug, "  cursor width: %" PRIu64, drmDev.caps.cursorWidth );
        mclog( LogLevel::Debug, "  cursor height: %" PRIu64, drmDev.caps.cursorHeight );
        mclog( LogLevel::Debug, "  addfb2 modifiers: %" PRIu64, drmDev.caps.addFB2Modifiers );
        mclog( LogLevel::Debug, "  page flip target: %" PRIu64, drmDev.caps.pageFlipTarget );
        mclog( LogLevel::Debug, "  crtc in vblank event: %" PRIu64, drmDev.caps.crtcInVblankEvent );
        mclog( LogLevel::Debug, "  sync obj: %" PRIu64, drmDev.caps.syncObj );
        mclog( LogLevel::Debug, "  sync obj timeline: %" PRIu64, drmDev.caps.syncObjTimeline );

        mclog( LogLevel::Debug, "  %d connectors, %d encoders, %d crtcs, %d framebuffers", drmDev.res->count_connectors, drmDev.res->count_encoders, drmDev.res->count_crtcs, drmDev.res->count_fbs );
        mclog( LogLevel::Debug, "  min resolution: %dx%d, max resolution: %dx%d", drmDev.res->min_width, drmDev.res->min_height, drmDev.res->max_width, drmDev.res->max_height );

        for( int i=0; i<drmDev.res->count_connectors; i++ )
        {
            auto conn = drmModeGetConnector( fd, drmDev.res->connectors[i] );
            if( !conn )
            {
                mclog( LogLevel::Debug, "  Skipping connector %d: failed to get connector", i );
                continue;
            }

            if( conn->connection == DRM_MODE_DISCONNECTED )
            {
                mclog( LogLevel::Debug, "  Connector %d: disconnected", i );
            }
            else
            {
                mclog( LogLevel::Debug, "  Connector %d: %dx%d mm", i, conn->mmWidth, conn->mmHeight );

                for( int j=0; j<conn->count_modes; j++ )
                {
                    const auto& mode = conn->modes[j];
                    mclog( LogLevel::Debug, "    Mode %d: %dx%d @ %.3f Hz", j, mode.hdisplay, mode.vdisplay, GetRefreshRate( mode ) / 1000.f );
                }
            }

            drmModeFreeConnector( conn );
        }

        if( isBoot )
        {
            m_drmDevices.insert( m_drmDevices.begin(), drmDev );
        }
        else
        {
            m_drmDevices.emplace_back( drmDev );
        }
    }

    sd_device_enumerator_unref( e );

    mclog( LogLevel::Info, "Found %zu DRM device(s)", m_drmDevices.size() );
}

BackendDrm::~BackendDrm()
{
    free( m_session );
    free( m_seat );
}

void BackendDrm::VulkanInit( VlkDevice& device, VkRenderPass renderPass, const VlkSwapchain& swapchain )
{
}

void BackendDrm::Run( const std::function<void()>& render )
{
}

void BackendDrm::Stop()
{
}

void BackendDrm::RenderCursor( VkCommandBuffer cmdBuf, CursorLogic& cursorLogic )
{
}

BackendDrm::operator VkSurfaceKHR() const
{
    return {};
}

int BackendDrm::PauseDevice( DbusMessage msg )
{
    mclog( LogLevel::Warning, "PauseDevice not implemented" );
    return 0;
}

int BackendDrm::ResumeDevice( DbusMessage msg )
{
    mclog( LogLevel::Warning, "ResumeDevice not implemented" );
    return 0;
}

int BackendDrm::PropertiesChanged( DbusMessage msg )
{
    mclog( LogLevel::Warning, "PropertiesChanged not implemented" );
    return 0;
}
