#include <stdlib.h>
#include <string.h>
#include <systemd/sd-device.h>
#include <systemd/sd-login.h>
#include <tracy/Tracy.hpp>
#include <unistd.h>

#include "BackendDrm.hpp"
#include "DbusLoginPaths.hpp"
#include "DrmDevice.hpp"
#include "backend/GpuDevice.hpp"
#include "dbus/DbusSession.hpp"
#include "server/Server.hpp"
#include "util/Panic.hpp"
#include "vulkan/VlkInstance.hpp"

#define DbusCallback( func ) [this] ( DbusMessage msg ) { return func( std::move( msg ) ); }

static bool GetCurrentSession( char*& session, char*& seat )
{
    if( const auto res = sd_pid_get_session( 0, &session ); res < 0 ) { mclog( LogLevel::Warning, "Failed to get session: %s", strerror( -res ) ); return false; }
    if( const auto res = sd_session_get_seat( session, &seat ); res < 0 ) { mclog( LogLevel::Warning, "Failed to get seat for session %s: %s", session, strerror( -res ) ); return false; }
    if( sd_seat_can_graphical( seat ) <= 0 ) { mclog( LogLevel::Warning, "Seat %s is not graphical", seat ); return false; }

    mclog( LogLevel::Info, "Using current session %s on seat %s", session, seat );
    return true;
}

static bool GetActiveLocalSession( const char* currentSession, char*& session, char*& seat )
{
    char** list;
    sd_uid_get_sessions( getuid(), 1, &list );
    if( !list ) return false;

    auto ptr = list;
    while( *ptr )
    {
        if( !currentSession || strcmp( currentSession, *ptr ) != 0 )
        {
            mclog( LogLevel::Debug, "Checking session %s", *ptr );
            if( sd_session_get_seat( *ptr, &seat ) >= 0 )
            {
                mclog( LogLevel::Debug, "Session %s is on seat %s", *ptr, seat );
                if( sd_seat_can_graphical( seat ) > 0 )
                {
                    mclog( LogLevel::Info, "Found remote active graphical session %s on seat %s", *ptr, seat );
                    session = strdup( *ptr );
                    break;
                }
                free( seat );
            }
        }
        ptr++;
    }

    ptr = list;
    while( *ptr ) free( *ptr++ );
    free( list );

    return session != nullptr;
}

BackendDrm::BackendDrm()
    : m_session( nullptr )
    , m_seat( nullptr )
{
    ZoneScoped;

    DbusSession bus;
    CheckPanic( bus, "Cannot continue without a valid Dbus connection." );

    if( !GetCurrentSession( m_session, m_seat ) )
    {
        auto currentSession = m_session;
        free( m_seat );
        m_session = nullptr;
        m_seat = nullptr;
        CheckPanic( GetActiveLocalSession( currentSession, m_session, m_seat ), "Failed to get an active local session" );
        free( currentSession );
    }

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

        try
        {
            auto drmDevice = std::make_unique<DrmDevice>( devName, bus, m_sessionPath.c_str() );

            if( isBoot )
            {
                m_drmDevices.insert( m_drmDevices.begin(), std::move( drmDevice ) );
            }
            else
            {
                m_drmDevices.emplace_back( std::move( drmDevice ) );
            }
        }
        catch( DrmDevice::DeviceException& e )
        {
            mclog( LogLevel::Debug, "Skipping DRM device %s: %s", devName, e.what() );
        }
    }

    sd_device_enumerator_unref( e );

    CheckPanic( !m_drmDevices.empty(), "No DRM devices found" );
    mclog( LogLevel::Info, "Found %zu DRM device(s)", m_drmDevices.size() );
}

BackendDrm::~BackendDrm()
{
    free( m_session );
    free( m_seat );
}

void BackendDrm::VulkanInit()
{
    ZoneScoped;

    auto it = m_drmDevices.begin();
    while( it != m_drmDevices.end() )
    {
        auto& drmDev = *it;
        auto physDev = drmDev->MatchPhysicalDevice();
        if( !physDev )
        {
            mclog( LogLevel::Warning, "Failed to resolve Vulkan device for DRM device '%s'", drmDev->Name().c_str() );
            it = m_drmDevices.erase( it );
        }
        else
        {
            try
            {
                auto gpu = std::make_shared<GpuDevice>( Server::Instance().VkInstance(), physDev );
                drmDev->SetGpuDevice( gpu );
                m_gpus.emplace_back( std::move( gpu ) );
                ++it;
            }
            catch( const std::exception& e )
            {
                mclog( LogLevel::Fatal, "Failed to initialize GPU: %s", e.what() );
                it = m_drmDevices.erase( it );
            }
        }
    }
}

void BackendDrm::Run()
{
}

void BackendDrm::Stop()
{
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
