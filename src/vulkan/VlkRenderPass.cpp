#include "VlkError.hpp"
#include "VlkRenderPass.hpp"

VlkRenderPass::VlkRenderPass( VkDevice device, const VkRenderPassCreateInfo& createInfo )
    : m_device( device )
{
    VkVerify( vkCreateRenderPass( device, &createInfo, nullptr, &m_renderPass ) );
}

VlkRenderPass::~VlkRenderPass()
{
    vkDestroyRenderPass( m_device, m_renderPass, nullptr );
}
