#pragma once

#include <stdexcept>
#include <stdint.h>
#include <string>

#include "util/NoCopy.hpp"

class DrmCrtc
{
public:
    struct CrtcException : public std::runtime_error { explicit CrtcException( const std::string& msg ) : std::runtime_error( msg ) {} };

    DrmCrtc( int fd, uint32_t id, uint32_t mask );
    ~DrmCrtc();

    NoCopy( DrmCrtc );

    void Enable();
    void Disable();

    [[nodiscard]] uint32_t Id() const { return m_id; }
    [[nodiscard]] bool IsUsed() const { return m_bufferId != 0; }
    [[nodiscard]] uint32_t Mask() const { return m_mask; }

private:
    int m_fd;
    uint32_t m_id;
    uint32_t m_bufferId;
    uint32_t m_mask;
};
