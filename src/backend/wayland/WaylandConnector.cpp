#include <array>
#include <tracy/Tracy.hpp>
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan.h>
#include <tracy/TracyVulkan.hpp>

#include "WaylandConnector.hpp"
#include "server/Server.hpp"
#include "server/Renderable.hpp"
#include "util/Tracy.hpp"
#include "vulkan/VlkCommandBuffer.hpp"
#include "vulkan/VlkDevice.hpp"
#include "vulkan/VlkError.hpp"
#include "vulkan/VlkFence.hpp"
#include "vulkan/VlkSemaphore.hpp"
#include "vulkan/VlkSwapchain.hpp"

WaylandConnector::WaylandConnector( VlkDevice& device, VkSurfaceKHR surface, uint32_t width, uint32_t height )
    : Connector( device )
{
    ZoneScoped;

    m_swapchain = std::make_unique<VlkSwapchain>( device, surface );

    m_width = m_swapchain->GetWidth();
    m_height = m_swapchain->GetHeight();
    m_format = m_swapchain->GetFormat();

    const auto& imageViews = m_swapchain->GetImageViews();
    const auto numImages = imageViews.size();

    m_frame.resize( numImages );
    for( size_t i=0; i<numImages; i++ )
    {
        m_frame[i].commandBuffer = std::make_unique<VlkCommandBuffer>( *device.GetCommandPool( QueueType::Graphic ), true );
        m_frame[i].imageAvailable = std::make_unique<VlkSemaphore>( device );
        m_frame[i].renderFinished = std::make_unique<VlkSemaphore>( device );
        m_frame[i].fence = std::make_unique<VlkFence>( device, VK_FENCE_CREATE_SIGNALED_BIT );
        m_frame[i].attachmentInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = imageViews[i],
            .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue = {{{ 0.25f, 0.25f, 0.25f, 1.0f }}}
        };
        m_frame[i].renderingInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea = { { 0, 0 }, { m_width, m_height } },
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &m_frame[i].attachmentInfo
        };
    }
}

WaylandConnector::~WaylandConnector()
{
    for( auto& frame : m_frame ) frame.fence->Wait();
}

void WaylandConnector::Render()
{
    const auto frameIdx = m_frameIndex;
    m_frameIndex = ( m_frameIndex + 1 ) % m_frame.size();

    auto& frame = m_frame[frameIdx];

    frame.fence->Wait();
    frame.fence->Reset();

    uint32_t imgIndex;
    auto res = vkAcquireNextImageKHR( m_device, *m_swapchain, UINT64_MAX, *frame.imageAvailable, VK_NULL_HANDLE, &imgIndex );

    //mclog( LogLevel::Warning, "Render frame [%" PRIu32 "], res: %s, image index: %" PRIu32, frameIdx, string_VkResult( res ), imgIndex );

    frame.commandBuffer->Reset();
    frame.commandBuffer->Begin( VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );

    m_swapchain->RenderBarrier( *frame.commandBuffer, imgIndex );
    vkCmdBeginRendering( *frame.commandBuffer, &frame.renderingInfo );

#ifdef TRACY_ENABLE
    auto tracyCtx = m_device.GetTracyContext();
    tracy::VkCtxScope* tracyScope = nullptr;
    if( tracyCtx ) ZoneVkNew( tracyCtx, tracyScope, *frame.commandBuffer, "Render", true );
#endif

    for( auto& renderable : Server::Instance().Renderables() ) renderable->Render( *this, *frame.commandBuffer );

#ifdef TRACY_ENABLE
    delete tracyScope;
#endif

    vkCmdEndRendering( *frame.commandBuffer );
    m_swapchain->PresentBarrier( *frame.commandBuffer, imgIndex );

#ifdef TRACY_ENABLE
    { auto tracyCtx = m_device.GetTracyContext(); if( tracyCtx ) TracyVkCollect( tracyCtx, *frame.commandBuffer ); }
#endif

    frame.commandBuffer->End();

    const std::array<VkSemaphore, 1> waitSemaphores = { *frame.imageAvailable };
    const std::array<VkSemaphore, 1> signalSemaphores = { *frame.renderFinished };
    constexpr std::array<VkPipelineStageFlags, 1> waitStages = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    const std::array<VkCommandBuffer, 1> commandBuffers = { *frame.commandBuffer };

    VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submitInfo.waitSemaphoreCount = waitSemaphores.size();
    submitInfo.pWaitSemaphores = waitSemaphores.data();
    submitInfo.pWaitDstStageMask = waitStages.data();
    submitInfo.commandBufferCount = commandBuffers.size();
    submitInfo.pCommandBuffers = commandBuffers.data();
    submitInfo.signalSemaphoreCount = signalSemaphores.size();
    submitInfo.pSignalSemaphores = signalSemaphores.data();

    VkVerify( vkQueueSubmit( m_device.GetQueue( QueueType::Graphic ), 1, &submitInfo, *frame.fence ) );

    const std::array<VkSwapchainKHR, 1> swapchains = { *m_swapchain };

    VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    presentInfo.waitSemaphoreCount = signalSemaphores.size();
    presentInfo.pWaitSemaphores = signalSemaphores.data();
    presentInfo.swapchainCount = swapchains.size();
    presentInfo.pSwapchains = swapchains.data();
    presentInfo.pImageIndices = &imgIndex;

    VkVerify( vkQueuePresentKHR( m_device.GetQueue( QueueType::Present ), &presentInfo ) );
}
