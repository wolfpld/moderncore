#include <fcntl.h>
#include <format>
#include <gbm.h>
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
#include "DrmPlane.hpp"
#include "dbus/DbusSession.hpp"
#include "server/GpuDevice.hpp"
#include "util/Logs.hpp"
#include "util/Panic.hpp"
#include "vulkan/ext/PciBus.hpp"

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
    if( !devMsg ) throw DeviceException( "Failed to take device" );
    m_dev = st.st_rdev;

    int fd;
    int paused;
    if( !devMsg.Read( "hb", &fd, &paused ) ) throw DeviceException( "Failed to read device file descriptor" );

    if( !drmIsKMS( fd ) ) throw DeviceException( "Not a KMS device" );

    m_fd = fcntl( fd, F_DUPFD_CLOEXEC, 0 );
    if( m_fd < 0 ) throw DeviceException( std::format( "Failed to dup fd: {}", strerror( errno ) ) );

    uint64_t cap;
    drmGetCap( m_fd, DRM_CAP_ADDFB2_MODIFIERS, &cap );
    if( !cap ) throw DeviceException( "Device does not support modifiers" );

    if( drmSetClientCap( m_fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1 ) != 0 ) throw DeviceException( "Failed to set universal planes cap" );
    if( drmSetClientCap( m_fd, DRM_CLIENT_CAP_ATOMIC, 1 ) != 0 ) throw DeviceException( "Failed to set atomic cap" );

    auto ver = drmGetVersion( m_fd );
    m_name = ver->name;
    mclog( LogLevel::Info, "DRM device %s: %s (%s)", devName, ver->name ? ver->name : "unknown", ver->desc ? ver->desc : "unknown" );
    drmFreeVersion( ver );

    drmDevicePtr drmDev;
    drmGetDevice( m_fd, &drmDev );
    if( drmDev->bustype != DRM_BUS_PCI ) throw DeviceException( "Not a PCI device" );
    const auto& pci = drmDev->businfo.pci;
    m_pci = { pci->domain, pci->bus, pci->dev, pci->func };
    mclog( LogLevel::Info, "  PCI bus: %04x:%02x:%02x.%x", pci->domain, pci->bus, pci->dev, pci->func );
    drmFreeDevice( &drmDev );

    m_res = drmModeGetResources( m_fd );
    if( !m_res ) throw DeviceException( "Failed to get resources" );

    auto planeRes = drmModeGetPlaneResources( m_fd );
    if( !planeRes ) throw DeviceException( "Failed to get plane resources" );

    mclog( LogLevel::Debug, "  %d connectors, %d crtcs, %d framebuffers, %d planes", m_res->count_connectors, m_res->count_crtcs, m_res->count_fbs, planeRes->count_planes );
    mclog( LogLevel::Debug, "  min resolution: %dx%d, max resolution: %dx%d", m_res->min_width, m_res->min_height, m_res->max_width, m_res->max_height );

    for( int i=0; i<planeRes->count_planes; i++ )
    {
        try
        {
            m_planes.emplace_back( std::make_shared<DrmPlane>( m_fd, planeRes->planes[i] ) );
        }
        catch( DrmPlane::PlaneException& e )
        {
            mclog( LogLevel::Warning, "  Skipping plane %d: %s", i, e.what() );
        }
    }
    drmModeFreePlaneResources( planeRes );

    for( int i=0; i<m_res->count_crtcs; i++ )
    {
        try
        {
            m_crtcs.emplace_back( std::make_shared<DrmCrtc>( m_fd, m_res->crtcs[i], 1 << i ) );
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
            m_connectors.emplace_back( std::make_shared<DrmConnector>( *this, m_res->connectors[i], m_res ) );
        }
        catch( DrmConnector::ConnectorException& e )
        {
            mclog( LogLevel::Warning, "  Skipping connector %d: %s", i, e.what() );
        }
    }

    m_gbm = gbm_create_device( m_fd );
    if( !m_gbm ) throw DeviceException( "Failed to create GBM device" );

    bool found = false;
    for( auto& conn : m_connectors )
    {
        if( !conn->IsConnected() ) continue;

        auto& mode = conn->GetBestDisplayMode();
        CheckPanic( conn->SetModeDrm( mode ), "Failed to set mode" );
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
    m_planes.clear();
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

bool DrmDevice::ResolveGpuDevice()
{
    auto physDev = GetPhysicalDeviceForPciBus( m_pci.domain, m_pci.bus, m_pci.dev, m_pci.func );
    if( !physDev ) return false;

    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties( physDev, &properties );
    mclog( LogLevel::Info, "DRM device '%s' matched to Vulkan device '%s'", m_name.c_str(), properties.deviceName );

    m_gpu = GetGpuDeviceForPhysicalDevice( physDev );
    if( !m_gpu ) return false;

    for( auto& conn : m_connectors ) conn->SetModeVulkan();
    return true;
}
