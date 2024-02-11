#pragma once

#include <stdexcept>
#include <stdint.h>

using drmModePlane = struct _drmModePlane;

class DrmPlane
{
public:
    struct PlaneException : public std::runtime_error { explicit PlaneException( const std::string& msg ) : std::runtime_error( msg ) {} };

    DrmPlane( int fd, uint32_t id );
    ~DrmPlane();

private:
    drmModePlane* m_plane;
};
