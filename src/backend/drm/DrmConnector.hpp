#ifndef __DRMCONNECTOR_HPP__
#define __DRMCONNECTOR_HPP__

#include <stdexcept>
#include <stdint.h>
#include <string>

using drmModeConnector = struct _drmModeConnector;

class DrmConnector
{
public:
    struct ConnectorException : public std::runtime_error { explicit ConnectorException( const std::string& msg ) : std::runtime_error( msg ) {} };

    DrmConnector( int fd, uint32_t id );
    ~DrmConnector();

private:
    bool m_connected;

    std::string m_name;
    std::string m_monitor;
};

#endif
