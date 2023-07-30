#ifndef __DRMCONNECTOR_HPP__
#define __DRMCONNECTOR_HPP__

#include <stdexcept>
#include <stdint.h>
#include <string>
#include <vector>

using drmModeConnector = struct _drmModeConnector;
using drmModeRes = struct _drmModeRes;

class DrmConnector
{
public:
    struct ConnectorException : public std::runtime_error { explicit ConnectorException( const std::string& msg ) : std::runtime_error( msg ) {} };

    DrmConnector( int fd, uint32_t id, const drmModeRes* res );
    ~DrmConnector();

    [[nodiscard]] uint32_t GetId() const { return m_id; }
    [[nodiscard]] bool IsConnected() const { return m_connected; }
    [[nodiscard]] const std::string& GetName() const { return m_name; }
    [[nodiscard]] const std::string& GetMonitor() const { return m_monitor; }
    [[nodiscard]] const std::vector<uint32_t>& GetCrtcs() const { return m_crtcs; }

private:
    uint32_t m_id;

    bool m_connected;

    std::string m_name;
    std::string m_monitor;

    std::vector<uint32_t> m_crtcs;
};

#endif
