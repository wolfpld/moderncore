#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
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
