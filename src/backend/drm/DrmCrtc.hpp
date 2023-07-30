#ifndef __DRMCRTC_HPP__
#define __DRMCRTC_HPP__

#include <stdexcept>
#include <stdint.h>
#include <string>

class DrmCrtc
{
public:
    struct CrtcException : public std::runtime_error { explicit CrtcException( const std::string& msg ) : std::runtime_error( msg ) {} };

    DrmCrtc( int fd, uint32_t id );
    ~DrmCrtc();

    void Enable();
    void Disable();

    [[nodiscard]] uint32_t GetId() const { return m_id; }
    [[nodiscard]] bool IsUsed() const { return m_inUse; }

private:
    int m_fd;
    uint32_t m_id;
    bool m_inUse;
    bool m_isAttached;
};

#endif
