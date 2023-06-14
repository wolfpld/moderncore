#include "VlkError.hpp"
#include "VlkImageView.hpp"

VlkImageView::VlkImageView( VkDevice device, const VkImageViewCreateInfo& createInfo )
    : m_device( device )
{
    VkVerify( vkCreateImageView( device, &createInfo, nullptr, &m_imageView ) );
}

VlkImageView::~VlkImageView()
{
    vkDestroyImageView( m_device, m_imageView, nullptr );
}
