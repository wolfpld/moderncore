#pragma once

#include <memory>
#include <vector>

#include "util/NoCopy.hpp"

class Backend;
class DbusSession;
class Display;
class GpuDevice;
class Renderable;
class VlkInstance;

class Server
{
public:
    Server();
    ~Server();

    static Server& Instance();

    NoCopy( Server );

    void Run();

private:
    void Render();

    void SetupGpus();

    std::unique_ptr<DbusSession> m_dbusSession;
    std::unique_ptr<VlkInstance> m_vkInstance;

    std::vector<std::shared_ptr<GpuDevice>> m_gpus;

    std::unique_ptr<Backend> m_backend;
    std::unique_ptr<Display> m_dpy;

    std::vector<std::shared_ptr<Renderable>> m_renderables;
};
