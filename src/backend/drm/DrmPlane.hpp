#pragma once

#include <stdexcept>
#include <stdint.h>
#include <vector>

#include "util/NoCopy.hpp"

using drmModePlane = struct _drmModePlane;

class DrmPlane
{
public:
    struct PlaneException : public std::runtime_error { explicit PlaneException( const std::string& msg ) : std::runtime_error( msg ) {} };

    DrmPlane( int fd, uint32_t id );
    ~DrmPlane();

    NoCopy( DrmPlane );

private:
    drmModePlane* m_plane;
    std::vector<uint64_t> m_modifiers;
};
