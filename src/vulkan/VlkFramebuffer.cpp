#include "VlkError.hpp"
#include "VlkFramebuffer.hpp"

VlkFramebuffer::VlkFramebuffer( VkDevice device, const VkFramebufferCreateInfo& createInfo )
    : m_device( device )
{
    VkVerify( vkCreateFramebuffer( device, &createInfo, nullptr, &m_framebuffer ) );
}

VlkFramebuffer::~VlkFramebuffer()
{
    vkDestroyFramebuffer( m_device, m_framebuffer, nullptr );
}
