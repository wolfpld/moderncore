#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <sys/types.h>
#include <vector>
#include <vulkan/vulkan.h>

#include "util/NoCopy.hpp"

using drmModeRes = struct _drmModeRes;
using drmModeModeInfo = struct _drmModeModeInfo;
struct gbm_device;
class DbusSession;
class DrmConnector;
class DrmCrtc;
class DrmPlane;
class GpuDevice;
class VlkPhysicalDevice;

class DrmDevice
{
public:
    struct DeviceException : public std::runtime_error { explicit DeviceException( const std::string& msg ) : std::runtime_error( msg ) {} };
    struct ModesetException : public std::runtime_error { explicit ModesetException( const std::string& msg ) : std::runtime_error( msg ) {} };

    DrmDevice( const char* devName, DbusSession& bus, const char* sessionPath );
    ~DrmDevice();

    NoCopy( DrmDevice );

    [[nodiscard]] auto& Crtcs() const { return m_crtcs; }
    [[nodiscard]] auto& Planes() const { return m_planes; }

    [[nodiscard]] auto Descriptor() const { return m_fd; }
    [[nodiscard]] auto& Name() const { return m_name; }

    [[nodiscard]] auto& Gpu() const { return m_gpu; }

    operator gbm_device*() const { return m_gbm; }

    std::shared_ptr<VlkPhysicalDevice> MatchPhysicalDevice();
    void SetGpuDevice( const std::shared_ptr<GpuDevice>& gpu );

private:
    struct PciBus
    {
        uint16_t domain;
        uint8_t bus;
        uint8_t dev;
        uint8_t func;
    };

    std::string m_sessionPath;

    dev_t m_dev;
    int m_fd;

    gbm_device* m_gbm;
    drmModeRes* m_res;

    std::shared_ptr<GpuDevice> m_gpu;

    std::vector<std::shared_ptr<DrmCrtc>> m_crtcs;
    std::vector<std::shared_ptr<DrmConnector>> m_connectors;
    std::vector<std::shared_ptr<DrmPlane>> m_planes;

    std::string m_name;
    PciBus m_pci;
};
