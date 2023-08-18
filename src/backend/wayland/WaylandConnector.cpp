#include "WaylandConnector.hpp"
#include "../../vulkan/VlkCommandBuffer.hpp"
#include "../../vulkan/VlkDevice.hpp"
#include "../../vulkan/VlkFence.hpp"
#include "../../vulkan/VlkFramebuffer.hpp"
#include "../../vulkan/VlkRenderPass.hpp"
#include "../../vulkan/VlkSemaphore.hpp"
#include "../../vulkan/VlkSwapchain.hpp"

WaylandConnector::WaylandConnector( const VlkDevice& device, VkSurfaceKHR surface )
    : m_swapchain( std::make_unique<VlkSwapchain>( device, surface ) )
{
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
    framebufferInfo.width = m_swapchain->GetWidth();
    framebufferInfo.height = m_swapchain->GetHeight();
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
}
