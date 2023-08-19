#pragma once

#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

#include "../../server/Connector.hpp"

class VlkCommandBuffer;
class VlkDevice;
class VlkFence;
class VlkFramebuffer;
class VlkRenderPass;
class VlkSemaphore;
class VlkSwapchain;

class WaylandConnector : public Connector
{
    struct FrameData
    {
        std::unique_ptr<VlkFramebuffer> framebuffer;
        std::unique_ptr<VlkCommandBuffer> commandBuffer;
        std::unique_ptr<VlkSemaphore> imageAvailable;
        std::unique_ptr<VlkSemaphore> renderFinished;
        std::unique_ptr<VlkFence> fence;
    };

public:
    WaylandConnector( const VlkDevice& device, VkSurfaceKHR surface );
    ~WaylandConnector() override;

    void Render() override;

private:
    std::unique_ptr<VlkSwapchain> m_swapchain;
    std::unique_ptr<VlkRenderPass> m_renderPass;

    const VlkDevice& m_device;

    std::vector<FrameData> m_frame;
    uint32_t m_frameIndex = 0;
};
