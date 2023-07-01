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

#include "BackendDrm.hpp"
#include "../../dbus/DbusSession.hpp"
#include "../../util/Panic.hpp"

#define DbusCallback( func ) [this] ( DbusMessage msg ) { return func( std::move( msg ) ); }

constexpr const char* LoginService = "org.freedesktop.login1";
constexpr const char* LoginPath = "/org/freedesktop/login1"; 
constexpr const char* LoginManagerIface = "org.freedesktop.login1.Manager";
constexpr const char* LoginSessionIface = "org.freedesktop.login1.Session";
constexpr const char* DbusPropertiesIface = "org.freedesktop.DBus.Properties";

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

        fd = fcntl( fd, F_DUPFD_CLOEXEC, 0 );
        if( fd < 0 )
        {
            mclog( LogLevel::Debug, "Skipping DRM device %s: failed to dup fd: %s", sysPath, strerror( errno ) );
            continue;
        }

        if( isBoot )
        {
            m_drmDevices.insert( m_drmDevices.begin(), fd );
        }
        else
        {
            m_drmDevices.emplace_back( fd );
        }

        mclog( LogLevel::Debug, "DRM device %s on seat %s%s", sysPath, seat, isBoot ? " (boot vga)" : "" );
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
