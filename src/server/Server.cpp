#include <stdlib.h>

#include "GpuDevices.hpp"
#include "GpuConnectors.hpp"
#include "Server.hpp"
#include "../backend/drm/BackendDrm.hpp"
#include "../backend/wayland/BackendWayland.hpp"
#include "../cursor/CursorLogic.hpp"
#include "../dbus/DbusSession.hpp"
#include "../plumbing/Display.hpp"
#include "../render/Background.hpp"
#include "../util/Logs.hpp"
#include "../vulkan/VlkInstance.hpp"

Server::Server()
    : m_dbusSession( std::make_unique<DbusSession>() )
{
    const auto waylandDpy = getenv( "WAYLAND_DISPLAY" );
    if( waylandDpy )
    {
        mclog( LogLevel::Info, "Running on Wayland display: %s", waylandDpy );
        m_vkInstance = std::make_unique<VlkInstance>( VlkInstanceType::Wayland );
    }
    else
    {
        m_vkInstance = std::make_unique<VlkInstance>( VlkInstanceType::Drm );
    }

    m_gpus = std::make_unique<GpuDevices>( *m_vkInstance );
    m_connectors = std::make_unique<GpuConnectors>();

    if( waylandDpy )
    {
        m_backend = std::make_unique<BackendWayland>( *m_vkInstance, *m_gpus, *m_connectors );
    }
    else
    {
        m_backend = std::make_unique<BackendDrm>( *m_vkInstance, *m_dbusSession );
    }

    m_dpy = std::make_unique<Display>();
    setenv( "WAYLAND_DISPLAY", m_dpy->Socket(), 1 );

    /*
    m_backend->VulkanInit( *m_vkDevice, *m_renderPass, *m_swapchain );

    m_background = std::make_unique<Background>( *m_vkDevice, *m_renderPass, m_swapchain->GetWidth(), m_swapchain->GetHeight() );
    m_cursorLogic = std::make_unique<CursorLogic>();
    */
}

Server::~Server()
{
}

void Server::Run()
{
    m_backend->Run( [this]{ Render(); } );
}

void Server::Render()
{
    /*
    const auto frameIdx = m_frameIdx;
    m_frameIdx = ( m_frameIdx + 1 ) % m_framebuffers.size();

    m_inFlightFences[frameIdx]->Wait();
    m_inFlightFences[frameIdx]->Reset();

    uint32_t imgIndex;
    auto res = vkAcquireNextImageKHR( *m_vkDevice, *m_swapchain, UINT64_MAX, *m_imgAvailSema[frameIdx], VK_NULL_HANDLE, &imgIndex );

    mclog( LogLevel::Warning, "Render frame [%" PRIu32 "], res: %s, image index: %" PRIu32, frameIdx, string_VkResult( res ), imgIndex );

    m_cmdBufs[frameIdx]->Reset();
    m_cmdBufs[frameIdx]->Begin( VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );

    VkRenderPassBeginInfo renderPassInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    renderPassInfo.renderPass = *m_renderPass;
    renderPassInfo.framebuffer = *m_framebuffers[imgIndex];
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = { m_swapchain->GetWidth(), m_swapchain->GetHeight() };
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &m_background->GetColor();

    vkCmdBeginRenderPass( *m_cmdBufs[frameIdx], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE );

    m_background->Render( *m_cmdBufs[frameIdx] );
    m_backend->RenderCursor( *m_cmdBufs[frameIdx], *m_cursorLogic );

    vkCmdEndRenderPass( *m_cmdBufs[frameIdx] );
    m_cmdBufs[frameIdx]->End();

    const std::array<VkSemaphore, 1> waitSemaphores = { *m_imgAvailSema[frameIdx] };
    constexpr std::array<VkPipelineStageFlags, 1> waitStages = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    const std::array<VkCommandBuffer, 1> commandBuffers = { *m_cmdBufs[frameIdx] };
    const std::array<VkSemaphore, 1> signalSemaphores = { *m_renderDoneSema[frameIdx] };

    VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submitInfo.waitSemaphoreCount = waitSemaphores.size();
    submitInfo.pWaitSemaphores = waitSemaphores.data();
    submitInfo.pWaitDstStageMask = waitStages.data();
    submitInfo.commandBufferCount = commandBuffers.size();
    submitInfo.pCommandBuffers = commandBuffers.data();
    submitInfo.signalSemaphoreCount = signalSemaphores.size();
    submitInfo.pSignalSemaphores = signalSemaphores.data();

    VkVerify( vkQueueSubmit( m_vkDevice->GetQueue( QueueType::Graphic ), 1, &submitInfo, *m_inFlightFences[frameIdx] ) );

    const std::array<VkSwapchainKHR, 1> swapchains = { *m_swapchain };

    VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    presentInfo.waitSemaphoreCount = signalSemaphores.size();
    presentInfo.pWaitSemaphores = signalSemaphores.data();
    presentInfo.swapchainCount = swapchains.size();
    presentInfo.pSwapchains = swapchains.data();
    presentInfo.pImageIndices = &imgIndex;

    vkQueuePresentKHR( m_vkDevice->GetQueue( QueueType::Graphic ), &presentInfo );
    */
}
