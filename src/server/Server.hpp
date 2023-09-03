#pragma once

#include <memory>
#include <vector>

#include "../util/NoCopy.hpp"

class Backend;
class DbusSession;
class Display;
class GpuState;
class Renderable;
class VlkInstance;

class Server
{
public:
    Server();
    ~Server();

    NoCopy( Server );

    void Run();

private:
    void Render();

    std::unique_ptr<DbusSession> m_dbusSession;
    std::unique_ptr<VlkInstance> m_vkInstance;
    std::unique_ptr<GpuState> m_gpuState;

    std::unique_ptr<Backend> m_backend;
    std::unique_ptr<Display> m_dpy;

    std::vector<std::shared_ptr<Renderable>> m_renderables;
};
