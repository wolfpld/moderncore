#include "VlkError.hpp"
#include "VlkSemaphore.hpp"

VlkSemaphore::VlkSemaphore( VkDevice device )
    : m_device( device )
{
    constexpr VkSemaphoreCreateInfo info = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkVerify( vkCreateSemaphore( device, &info, nullptr, &m_semaphore ) );
}

VlkSemaphore::~VlkSemaphore()
{
    vkDestroySemaphore( m_device, m_semaphore, nullptr );
}
