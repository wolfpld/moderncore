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

    [[nodiscard]] uint32_t GetId() const { return m_id; }

private:
    uint32_t m_id;
};

#endif
