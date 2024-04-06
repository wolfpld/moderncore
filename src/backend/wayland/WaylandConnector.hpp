#pragma once

#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

#include "server/Connector.hpp"
#include "util/NoCopy.hpp"

class VlkCommandBuffer;
class VlkFence;
class VlkSemaphore;
class VlkSwapchain;

class WaylandConnector : public Connector
{
    struct FrameData
    {
        std::unique_ptr<VlkCommandBuffer> commandBuffer;
        std::unique_ptr<VlkSemaphore> imageAvailable;
        std::unique_ptr<VlkSemaphore> renderFinished;
        std::unique_ptr<VlkFence> fence;
        VkRenderingAttachmentInfo attachmentInfo;
        VkRenderingInfo renderingInfo;
    };

public:
    WaylandConnector( VlkDevice& device, VkSurfaceKHR surface, uint32_t width, uint32_t height );
    ~WaylandConnector() override;

    NoCopy( WaylandConnector );

    void Render() override;

private:
    std::unique_ptr<VlkSwapchain> m_swapchain;

    std::vector<FrameData> m_frame;
    uint32_t m_frameIndex = 0;
};
