#include <array>
#include <tracy/Tracy.hpp>
#include <vulkan/vk_enum_string_helper.h>

#include "WaylandConnector.hpp"
#include "server/Renderable.hpp"
#include "vulkan/VlkCommandBuffer.hpp"
#include "vulkan/VlkDevice.hpp"
#include "vulkan/VlkError.hpp"
#include "vulkan/VlkFence.hpp"
#include "vulkan/VlkFramebuffer.hpp"
#include "vulkan/VlkRenderPass.hpp"
#include "vulkan/VlkSemaphore.hpp"
#include "vulkan/VlkSwapchain.hpp"

WaylandConnector::WaylandConnector( VlkDevice& device, VkSurfaceKHR surface )
    : Connector( device )
{
    ZoneScoped;

    m_swapchain = std::make_unique<VlkSwapchain>( device, surface );

    m_width = m_swapchain->GetWidth();
    m_height = m_swapchain->GetHeight();

    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = m_swapchain->GetFormat();
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpassDesc = {};
    subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDesc.colorAttachmentCount = 1;
    subpassDesc.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpassDesc;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    m_renderPass = std::make_unique<VlkRenderPass>( device, renderPassInfo );

    const auto& imageViews = m_swapchain->GetImageViews();
    VkFramebufferCreateInfo framebufferInfo = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
    framebufferInfo.renderPass = *m_renderPass;
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.width = m_width;
    framebufferInfo.height = m_height;
    framebufferInfo.layers = 1;

    const auto numImages = imageViews.size();
    m_frame.resize( numImages );
    for( size_t i=0; i<numImages; i++ )
    {
        framebufferInfo.pAttachments = &imageViews[i];
        m_frame[i].framebuffer = std::make_unique<VlkFramebuffer>( device, framebufferInfo );
        m_frame[i].commandBuffer = std::make_unique<VlkCommandBuffer>( *device.GetCommandPool( QueueType::Graphic ), true );
        m_frame[i].imageAvailable = std::make_unique<VlkSemaphore>( device );
        m_frame[i].renderFinished = std::make_unique<VlkSemaphore>( device );
        m_frame[i].fence = std::make_unique<VlkFence>( device, VK_FENCE_CREATE_SIGNALED_BIT );
    }
}

WaylandConnector::~WaylandConnector()
{
    for( auto& frame : m_frame ) frame.fence->Wait();
}

void WaylandConnector::Render( const std::vector<std::shared_ptr<Renderable>>& renderables )
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

    VkRenderPassBeginInfo renderPassInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    renderPassInfo.renderPass = *m_renderPass;
    renderPassInfo.framebuffer = *frame.framebuffer;
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = { m_swapchain->GetWidth(), m_swapchain->GetHeight() };
    renderPassInfo.clearValueCount = 1;
    VkClearValue clearValue = {{{ 0.25f, 0.25f, 0.25f, 1.0f }}};
    renderPassInfo.pClearValues = &clearValue;

    vkCmdBeginRenderPass( *frame.commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE );

    for( auto& renderable : renderables ) renderable->Render( *this, *frame.commandBuffer );

    vkCmdEndRenderPass( *frame.commandBuffer );
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
