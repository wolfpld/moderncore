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

private:
    uint32_t m_id;

    bool m_connected;

    std::string m_name;
    std::string m_monitor;

    std::vector<uint32_t> m_crtcs;
};

#endif
