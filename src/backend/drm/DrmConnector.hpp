#pragma once

#include <memory>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <vector>

#include "util/NoCopy.hpp"

using drmModeConnector = struct _drmModeConnector;
using drmModeRes = struct _drmModeRes;
using drmModeModeInfo = struct _drmModeModeInfo;
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

    [[nodiscard]] const drmModeModeInfo& GetBestDisplayMode() const;

    [[nodiscard]] uint32_t GetId() const { return m_id; }
    [[nodiscard]] bool IsConnected() const { return m_connected; }
    [[nodiscard]] const std::string& GetName() const { return m_name; }
    [[nodiscard]] const std::string& GetMonitor() const { return m_monitor; }
    [[nodiscard]] const std::vector<uint32_t>& GetCrtcs() const { return m_crtcs; }

private:
    const std::shared_ptr<DrmPlane>& GetPlaneForCrtc( const DrmCrtc& crtc );

    uint32_t m_id;

    gbm_bo* m_bo;
    uint32_t m_fbId;
    bool m_connected;

    std::string m_name;
    std::string m_monitor;

    std::shared_ptr<DrmCrtc> m_crtc;
    std::shared_ptr<DrmPlane> m_plane;

    std::vector<uint32_t> m_crtcs;
    std::vector<drmModeModeInfo> m_modes;

    DrmDevice& m_device;
};
