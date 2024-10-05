#pragma once

#include <memory>

#include "util/NoCopy.hpp"

class Backend;
class DbusSession;
class Display;
class GpuDevice;
class VlkInstance;

class Server
{
public:
    explicit Server( bool enableValidation );
    ~Server();

    static Server& Instance();

    NoCopy( Server );

    void Run();

    [[nodiscard]] auto& VkInstance() const { return *m_vkInstance; }

private:
    void SetupGpus( bool skipSoftware );

    std::unique_ptr<DbusSession> m_dbusSession;
    std::unique_ptr<VlkInstance> m_vkInstance;

    std::unique_ptr<Backend> m_backend;
    std::unique_ptr<Display> m_dpy;
};
