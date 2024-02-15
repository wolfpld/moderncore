#pragma once

#include <stdexcept>
#include <stdint.h>
#include <string>

#include "util/NoCopy.hpp"

class DrmCrtc
{
public:
    struct CrtcException : public std::runtime_error { explicit CrtcException( const std::string& msg ) : std::runtime_error( msg ) {} };

    DrmCrtc( int fd, uint32_t id );
    ~DrmCrtc();

    NoCopy( DrmCrtc );

    void Enable();
    void Disable();

    [[nodiscard]] uint32_t GetId() const { return m_id; }
    [[nodiscard]] bool IsUsed() const { return m_inUse; }

private:
    int m_fd;
    uint32_t m_id;
    uint32_t m_bufferId;
    bool m_inUse;
};
