#ifndef __SERVER_HPP__
#define __SERVER_HPP__

#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

class Backend;
class Display;
class VlkCommandBuffer;
class VlkCommandPool;
class VlkDevice;
class VlkFence;
class VlkFramebuffer;
class VlkInstance;
class VlkRenderPass;
class VlkSemaphore;
class VlkSwapchain;

class Server
{
public:
    Server();
    ~Server();

    void Run();

private:
    void Render();

    std::unique_ptr<VlkInstance> m_vkInstance;
    std::unique_ptr<VlkDevice> m_vkDevice;
    std::unique_ptr<Backend> m_backend;
    std::unique_ptr<Display> m_dpy;

    std::unique_ptr<VlkSwapchain> m_swapchain;
    std::unique_ptr<VlkRenderPass> m_renderPass;

    std::vector<std::unique_ptr<VlkFramebuffer>> m_framebuffers;
    std::vector<std::unique_ptr<VlkCommandBuffer>> m_cmdBufs;
    std::vector<std::unique_ptr<VlkSemaphore>> m_imgAvailSema;
    std::vector<std::unique_ptr<VlkSemaphore>> m_renderDoneSema;
    std::vector<std::unique_ptr<VlkFence>> m_inFlightFences;

    uint32_t m_frameIdx = 0;
};

#endif
