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

VlkSwapchain::VlkSwapchain( const VlkDevice& device, VkSurfaceKHR surface, const VkExtent2D& extent, VkSwapchainKHR oldSwapchain )
    : m_properties( *device.GetPhysicalDevice(), surface )
    , m_format { VK_FORMAT_UNDEFINED }
    , m_presentMode( VK_PRESENT_MODE_FIFO_KHR )
    , m_device( device )
{
    if( GetLogLevel() <= LogLevel::Debug ) PrintSwapchainProperties( m_properties );

    for( auto& format: m_properties.GetFormats() )
    {
        for( auto& valid : SdrSwapchainFormats )
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

    VkSwapchainCreateInfoKHR createInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = imageCount,
        .imageFormat = m_format.format,
        .imageColorSpace = m_format.colorSpace,
        .imageExtent = m_extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .preTransform = caps.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = m_presentMode,
        .clipped = VK_TRUE,
        .oldSwapchain = oldSwapchain
    };

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

    VkImageViewCreateInfo viewCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = m_format.format,
        .components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };

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
    VkImageMemoryBarrier2 barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .oldLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = m_images[imageIndex],
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    VkDependencyInfo deps = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier
    };
    vkCmdPipelineBarrier2( cmd, &deps );
}

void VlkSwapchain::RenderBarrier( VkCommandBuffer cmd, uint32_t imageIndex )
{
    VkImageMemoryBarrier2 barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = m_images[imageIndex],
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    VkDependencyInfo deps = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier
    };
    vkCmdPipelineBarrier2( cmd, &deps );
}
