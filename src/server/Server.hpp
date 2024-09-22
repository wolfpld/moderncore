#pragma once

#include <memory>
#include <vector>

#include "util/NoCopy.hpp"

class Backend;
class DbusSession;
class Display;
class GpuDevice;
class TaskDispatch;
class VlkInstance;

class Server
{
public:
    explicit Server( bool enableValidation );
    ~Server();

    static Server& Instance();

    NoCopy( Server );

    void Run();

    [[nodiscard]] auto& Dispatch() const { return *m_dispatch; }
    [[nodiscard]] auto& VkInstance() const { return *m_vkInstance; }
    [[nodiscard]] auto& Gpus() const { return m_gpus; }

private:
    void SetupGpus( bool skipSoftware );

    std::unique_ptr<TaskDispatch> m_dispatch;

    std::unique_ptr<DbusSession> m_dbusSession;
    std::unique_ptr<VlkInstance> m_vkInstance;

    std::vector<std::shared_ptr<GpuDevice>> m_gpus;

    std::unique_ptr<Backend> m_backend;
    std::unique_ptr<Display> m_dpy;
};
