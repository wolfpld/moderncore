#ifndef __DRMCONNECTOR_HPP__
#define __DRMCONNECTOR_HPP__

#include <stdexcept>
#include <stdint.h>
#include <string>
#include <vector>

using drmModeConnector = struct _drmModeConnector;
using drmModeRes = struct _drmModeRes;
using drmModeModeInfo = struct _drmModeModeInfo;
class DrmDevice;
struct gbm_bo;

class DrmConnector
{
public:
    struct ConnectorException : public std::runtime_error { explicit ConnectorException( const std::string& msg ) : std::runtime_error( msg ) {} };

    DrmConnector( DrmDevice& device, uint32_t id, const drmModeRes* res );
    ~DrmConnector();

    bool SetMode( const drmModeModeInfo& mode );

    [[nodiscard]] const drmModeModeInfo& GetBestDisplayMode() const;

    [[nodiscard]] uint32_t GetId() const { return m_id; }
    [[nodiscard]] bool IsConnected() const { return m_connected; }
    [[nodiscard]] const std::string& GetName() const { return m_name; }
    [[nodiscard]] const std::string& GetMonitor() const { return m_monitor; }
    [[nodiscard]] const std::vector<uint32_t>& GetCrtcs() const { return m_crtcs; }

private:
    uint32_t m_id;

    gbm_bo* m_bo;
    uint32_t m_fbId;
    bool m_connected;

    std::string m_name;
    std::string m_monitor;

    std::vector<uint32_t> m_crtcs;
    std::vector<drmModeModeInfo> m_modes;

    DrmDevice& m_device;
};

#endif
