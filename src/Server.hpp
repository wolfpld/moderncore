#ifndef __SERVER_HPP__
#define __SERVER_HPP__

#include <memory>

class Backend;
class Display;
class VlkDevice;
class VlkInstance;

class Server
{
public:
    Server();
    ~Server();

    void Run();

private:
    std::unique_ptr<VlkInstance> m_vkInstance;
    std::unique_ptr<Backend> m_backend;
    std::unique_ptr<VlkDevice> m_vkDevice;
    std::unique_ptr<Display> m_dpy;
};

#endif
