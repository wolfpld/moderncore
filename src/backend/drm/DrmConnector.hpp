#pragma once

#include <memory>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <vector>
#include <xf86drmMode.h>

#include "util/NoCopy.hpp"

class DrmBuffer;
class DrmCrtc;
class DrmDevice;
class DrmPlane;
struct gbm_bo;

class DrmConnector
{
public:
    struct ConnectorException : public std::runtime_error { explicit ConnectorException( const std::string& msg ) : std::runtime_error( msg ) {} };

    DrmConnector( DrmDevice& device, uint32_t id, const drmModeRes* res );
    ~DrmConnector();

    NoCopy( DrmConnector );

    bool SetMode( const drmModeModeInfo& mode );

    bool SetModeDrm( const drmModeModeInfo& mode );
    bool SetModeVulkan();

    [[nodiscard]] const drmModeModeInfo& BestDisplayMode() const;

    [[nodiscard]] uint32_t Id() const { return m_id; }
    [[nodiscard]] bool IsConnected() const { return m_connected; }
    [[nodiscard]] const std::string& Name() const { return m_name; }
    [[nodiscard]] const std::string& Monitor() const { return m_monitor; }
    [[nodiscard]] const std::vector<uint32_t>& Crtcs() const { return m_crtcs; }

private:
    const std::shared_ptr<DrmPlane>& GetPlaneForCrtc( const DrmCrtc& crtc );

    uint32_t m_id;
    bool m_connected;

    std::string m_name;
    std::string m_monitor;

    std::shared_ptr<DrmCrtc> m_crtc;
    std::shared_ptr<DrmPlane> m_plane;

    std::vector<uint32_t> m_crtcs;
    std::vector<drmModeModeInfo> m_modes;
    std::vector<uint64_t> m_modifiers;

    std::vector<std::shared_ptr<DrmBuffer>> m_buffers;

    DrmDevice& m_device;
    drmModeModeInfo m_mode;
};
