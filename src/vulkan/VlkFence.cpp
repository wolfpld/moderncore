#include "VlkError.hpp"
#include "VlkFence.hpp"

VlkFence::VlkFence( VkDevice device, VkFenceCreateFlags flags )
    : m_device( device )
{
    VkFenceCreateInfo createInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    createInfo.flags = flags;
    VkVerify( vkCreateFence( device, &createInfo, nullptr, &m_fence ) );
}

VlkFence::~VlkFence()
{
    vkDestroyFence( m_device, m_fence, nullptr );
}

VkResult VlkFence::Wait( uint64_t timeout )
{
    return vkWaitForFences( m_device, 1, &m_fence, VK_TRUE, timeout );
}

void VlkFence::Reset()
{
    VkVerify( vkResetFences( m_device, 1, &m_fence ) );
}
