#include <algorithm>
#include <array>
#include <stdint.h>
#include <vector>
#include <vulkan/vk_enum_string_helper.h>

#include "VlkDevice.hpp"
#include "VlkError.hpp"
#include "VlkSwapchain.hpp"
#include "VlkSwapchainFormats.hpp"
#include "util/Logs.hpp"
#include "util/Panic.hpp"

static void PrintSwapchainProperties( const VlkSwapchainProperties &properties )
{
    const auto caps = properties.GetCapabilities();
    mclog( LogLevel::Debug, "Swapchain image count: min %d, max %d", caps.minImageCount, caps.maxImageCount );
    mclog( LogLevel::Debug, "Swapchain surface formats:" );
    for( auto& format : properties.GetFormats() )
    {
        mclog( LogLevel::Debug, "  %s / %s", string_VkFormat( format.format ), string_VkColorSpaceKHR( format.colorSpace ) );
    }
    mclog( LogLevel::Debug, "Swapchain present modes:" );
    for( auto& mode : properties.GetPresentModes() )
    {
        mclog( LogLevel::Debug, "  %s", string_VkPresentModeKHR( mode ) );
    }
}

VlkSwapchain::VlkSwapchain( const VlkDevice& device, VkSurfaceKHR surface, const VkExtent2D& extent )
    : m_properties( *device.GetPhysicalDevice(), surface )
    , m_format { VK_FORMAT_UNDEFINED }
    , m_presentMode( VK_PRESENT_MODE_FIFO_KHR )
    , m_device( device )
{
#ifdef DEBUG
    PrintSwapchainProperties( m_properties );
#endif

    for( auto& format: m_properties.GetFormats() )
    {
        for( auto& valid : SupportedSwapchainFormats )
        {
            if( format.format == valid.format && format.colorSpace == valid.colorSpace )
            {
                m_format = format;
                break;
            }
        }
        if( m_format.format != VK_FORMAT_UNDEFINED ) break;
    }
    CheckPanic( m_format.format != VK_FORMAT_UNDEFINED, "No valid swapchain format found" );

    mclog( LogLevel::Info, "Swapchain format: %s / %s", string_VkFormat( m_format.format ), string_VkColorSpaceKHR( m_format.colorSpace ) );

    const auto caps = m_properties.GetCapabilities();
    m_extent.width = caps.currentExtent.width == UINT32_MAX ?
        std::clamp( extent.width, caps.minImageExtent.width, caps.maxImageExtent.width ) : caps.currentExtent.width;
    m_extent.height = caps.currentExtent.height == UINT32_MAX ?
        std::clamp( extent.height, caps.minImageExtent.height, caps.maxImageExtent.height ) : caps.currentExtent.height;

    mclog( LogLevel::Info, "Swapchain extent: %ux%u", m_extent.width, m_extent.height );

    auto imageCount = std::max( 2u, caps.minImageCount );
    if( caps.maxImageCount > 0 && imageCount > caps.maxImageCount ) imageCount = caps.maxImageCount;

    mclog( LogLevel::Info, "Swapchain image count: %u", imageCount );

    const auto& graphicQueue = device.GetQueueInfo( QueueType::Graphic );
    const auto& presentQueue = device.GetQueueInfo( QueueType::Present );
    const std::array queues = { uint32_t( graphicQueue.idx ), uint32_t( presentQueue.idx ) };

    VkSwapchainCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = m_format.format;
    createInfo.imageColorSpace = m_format.colorSpace;
    createInfo.imageExtent = m_extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.preTransform = caps.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = m_presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if( graphicQueue.sharePresent )
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    else
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = static_cast<uint32_t>( queues.size() );
        createInfo.pQueueFamilyIndices = queues.data();
    }

    VkVerify( vkCreateSwapchainKHR( device, &createInfo, nullptr, &m_swapchain ) );

    uint32_t swapchainImageCount = 0;
    VkVerify( vkGetSwapchainImagesKHR( device, m_swapchain, &swapchainImageCount, nullptr ) );
    m_images.resize( swapchainImageCount );
    VkVerify( vkGetSwapchainImagesKHR( device, m_swapchain, &swapchainImageCount, m_images.data() ) );

    if( swapchainImageCount != imageCount ) mclog( LogLevel::Warning, "Swapchain created with %u images", swapchainImageCount );

    VkImageViewCreateInfo viewCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewCreateInfo.format = m_format.format;
    viewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewCreateInfo.subresourceRange.baseMipLevel = 0;
    viewCreateInfo.subresourceRange.levelCount = 1;
    viewCreateInfo.subresourceRange.baseArrayLayer = 0;
    viewCreateInfo.subresourceRange.layerCount = 1;

    m_imageViews.resize( swapchainImageCount );
    for( uint32_t i=0; i<swapchainImageCount; ++i )
    {
        viewCreateInfo.image = m_images[i];
        VkVerify( vkCreateImageView( device, &viewCreateInfo, nullptr, &m_imageViews[i] ) );
    }
}

VlkSwapchain::~VlkSwapchain()
{
    vkDestroySwapchainKHR( m_device, m_swapchain, nullptr );
    for( auto& view : m_imageViews ) vkDestroyImageView( m_device, view, nullptr );
}

void VlkSwapchain::PresentBarrier( VkCommandBuffer cmd, uint32_t imageIndex )
{
    VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_images[imageIndex];
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier );
}

void VlkSwapchain::RenderBarrier( VkCommandBuffer cmd, uint32_t imageIndex )
{
    VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_images[imageIndex];
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier );
}
