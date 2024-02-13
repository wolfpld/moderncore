#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <sys/types.h>
#include <vector>
#include <vulkan/vulkan.h>

using drmModeRes = struct _drmModeRes;
using drmModeModeInfo = struct _drmModeModeInfo;
struct gbm_device;
class DbusSession;
class DrmConnector;
class DrmCrtc;
class DrmPlane;
class GpuDevice;

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

    void ResolveGpuDevice();

private:
    std::string m_sessionPath;

    dev_t m_dev;
    int m_fd;

    gbm_device* m_gbm;
    drmModeRes* m_res;

    std::shared_ptr<GpuDevice> m_gpu;

    std::vector<std::unique_ptr<DrmCrtc>> m_crtcs;
    std::vector<std::unique_ptr<DrmConnector>> m_connectors;
    std::vector<std::unique_ptr<DrmPlane>> m_planes;

    // Keep temporary things last
    VkPhysicalDevice m_tmpPhysDev;
};
