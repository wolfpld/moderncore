#include <algorithm>
#include <bit>
#include <format>
#include <gbm.h>
#include <tracy/Tracy.hpp>

extern "C" {
#include <libdisplay-info/info.h>
}

#include "DrmBuffer.hpp"
#include "DrmDevice.hpp"
#include "DrmConnector.hpp"
#include "DrmCrtc.hpp"
#include "DrmPlane.hpp"
#include "DrmProperties.hpp"
#include "server/GpuDevice.hpp"
#include "util/Logs.hpp"
#include "util/Panic.hpp"
#include "vulkan/VlkError.hpp"

constexpr static int BufferNum = 3;

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
    , m_monitor( "unknown" )
    , m_device( device )
    , m_mode{}
{
    ZoneScoped;

    auto conn = drmModeGetConnector( device.Descriptor(), id );
    if( !conn ) throw ConnectorException( "Failed to get connector" );
    CheckPanic( id == conn->connector_id, "Connector ID mismatch" );

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
        CheckPanic( bit < res->count_crtcs, "CRTC index out of bounds" );
        m_crtcs.push_back( bit );
    }

    if( !m_connected )
    {
        mclog( LogLevel::Info, "  Connector %s: disconnected", m_name.c_str() );
    }
    else
    {
        DrmProperties props( device.Descriptor(), conn->connector_id, DRM_MODE_OBJECT_CONNECTOR );
        if( props )
        {
            auto prop = props["EDID"];
            if( prop )
            {
                auto blob = prop.Blob();
                if( blob )
                {
                    auto info = di_info_parse_edid( blob->data, blob->length );
                    if( info )
                    {
                        m_monitor = di_info_get_model( info );
                        di_info_destroy( info );
                    }
                }
            }
        }

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
}

bool DrmConnector::SetMode( const drmModeModeInfo& mode )
{
    ZoneScoped;

    if( SetModeDrm( mode ) && SetModeVulkan() ) return true;

    m_crtc.reset();
    m_plane.reset();
    m_mode = {};

    return false;
}

bool DrmConnector::SetModeDrm( const drmModeModeInfo& mode )
{
    ZoneScoped;
    CheckPanic( m_connected, "Connector is not connected" );

    mclog( LogLevel::Info, "  Setting connector %s to %dx%d @ %d Hz", m_name.c_str(), mode.hdisplay, mode.vdisplay, mode.vrefresh );

    auto& crtcs = m_device.GetCrtcs();
    auto it = std::find_if( crtcs.begin(), crtcs.end(), []( const auto& c ) { return !c->IsUsed(); } );
    if( it == crtcs.end() ) return false;

    m_plane = GetPlaneForCrtc( **it );
    if( !m_plane ) return false;

    m_buffers.clear();
    m_modifiers.clear();
    m_mode = mode;
    m_crtc = *it;

    return true;
}

bool DrmConnector::SetModeVulkan()
{
    ZoneScoped;

    CheckPanic( m_plane, "m_plane is nullptr" );
    CheckPanic( m_crtc, "m_crtc is nullptr" );
    CheckPanic( m_modifiers.empty(), "m_modifiers is empty" );
    CheckPanic( m_buffers.empty(), "m_buffers is empty" );

    VkPhysicalDeviceImageDrmFormatModifierInfoEXT modInfo = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_DRM_FORMAT_MODIFIER_INFO_EXT };

    VkPhysicalDeviceExternalImageFormatInfo extInfo = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO };
    extInfo.pNext = &modInfo;
    extInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;

    VkPhysicalDeviceImageFormatInfo2 formatInfo = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2 };
    formatInfo.pNext = &extInfo;
    formatInfo.format = VK_FORMAT_B8G8R8A8_SRGB;
    formatInfo.type = VK_IMAGE_TYPE_2D;
    formatInfo.tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
    formatInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    VkExternalImageFormatProperties extProp = { VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES };

    VkImageFormatProperties2 prop = { VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2 };
    prop.pNext = &extProp;

    auto& modifiers = m_plane->Modifiers();
    for( auto mod : modifiers )
    {
        modInfo.drmFormatModifier = mod;
        auto res = vkGetPhysicalDeviceImageFormatProperties2( m_device.GetGpu()->Device(), &formatInfo, &prop );
        if( res == VK_ERROR_FORMAT_NOT_SUPPORTED ) continue;
        VkVerify( res );
        if( extProp.externalMemoryProperties.externalMemoryFeatures & VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT )
        {
            m_modifiers.emplace_back( mod );
        }
    }

    if( m_modifiers.empty() )
    {
        mclog( LogLevel::Warning, "  No supported modifiers found" );
        return false;
    }

    for( int i=0; i<BufferNum; i++ )
    {
        m_buffers.emplace_back( std::make_shared<DrmBuffer>( m_device, m_mode, m_modifiers ) );
    }

#if 0
    // todo
    assert( !m_bo );

    m_bo = gbm_bo_create( m_device, m_mode.hdisplay, m_mode.vdisplay, GBM_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING );
    if( !m_bo ) return false;

    if( drmModeAddFB( m_device.Descriptor(), m_mode.hdisplay, m_mode.vdisplay, 24, 32, gbm_bo_get_stride( m_bo ), gbm_bo_get_handle( m_bo ).u32, &m_fbId ) != 0 )
    {
        gbm_bo_destroy( m_bo );
        m_bo = nullptr;
        return false;
    }

    auto _mode = m_mode;
    if( drmModeSetCrtc( m_device.Descriptor(), m_crtc->GetId(), m_fbId, 0, 0, &m_id, 1, &_mode ) != 0 )
    {
        drmModeRmFB( m_device.Descriptor(), m_fbId );
        gbm_bo_destroy( m_bo );
        m_bo = nullptr;
        return false;
    }

    m_crtc->Enable();
#endif

    return true;
}

const drmModeModeInfo& DrmConnector::GetBestDisplayMode() const
{
    CheckPanic( m_connected, "Connector is not connected");
    CheckPanic( !m_modes.empty(), "No modes available" );

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

const std::shared_ptr<DrmPlane>& DrmConnector::GetPlaneForCrtc( const DrmCrtc& crtc )
{
    auto& planes = m_device.GetPlanes();
    for( auto& plane : planes )
    {
        auto& p = *plane->Plane();
        if( p.possible_crtcs & crtc.Mask() )
        {
            if( plane->Type() == DRM_PLANE_TYPE_PRIMARY )
            {
                mclog( LogLevel::Debug, "  Using plane %d for connector %s", p.plane_id, m_name.c_str() );
                return plane;
            }
        }
    }

    static std::shared_ptr<DrmPlane> empty;
    return empty;
}
