#pragma once

#include <memory>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <sys/types.h>
#include <vector>

using drmModeRes = struct _drmModeRes;
using drmModeModeInfo = struct _drmModeModeInfo;
struct gbm_device;
class DbusSession;
class DrmConnector;
class DrmCrtc;
class DrmPlane;

struct DrmCaps
{
    uint64_t dumbBuffer;
    uint64_t vblankHighCrtc;
    uint64_t dumbPreferredDepth;
    uint64_t dumbPreferShadow;
    uint64_t prime;
    uint64_t timestampMonotonic;
    uint64_t asyncPageFlip;
    uint64_t cursorWidth;
    uint64_t cursorHeight;
    uint64_t pageFlipTarget;
    uint64_t crtcInVblankEvent;
    uint64_t syncObj;
    uint64_t syncObjTimeline;
};

class DrmDevice
{
public:
    struct DeviceException : public std::runtime_error { explicit DeviceException( const std::string& msg ) : std::runtime_error( msg ) {} };
    struct ModesetException : public std::runtime_error { explicit ModesetException( const std::string& msg ) : std::runtime_error( msg ) {} };

    DrmDevice( const char* devName, DbusSession& bus, const char* sessionPath );
    ~DrmDevice();

    [[nodiscard]] const std::vector<std::unique_ptr<DrmCrtc>>& GetCrtcs() const { return m_crtcs; }
    [[nodiscard]] int Descriptor() const { return m_fd; }

    operator gbm_device*() const { return m_gbm; }

private:
    std::string m_sessionPath;

    dev_t m_dev;
    int m_fd;

    gbm_device* m_gbm;

    DrmCaps m_caps;
    drmModeRes* m_res;

    std::vector<std::unique_ptr<DrmCrtc>> m_crtcs;
    std::vector<std::unique_ptr<DrmConnector>> m_connectors;
    std::vector<std::unique_ptr<DrmPlane>> m_planes;
};
