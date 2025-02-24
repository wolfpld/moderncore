#include <tracy/Tracy.hpp>

#include "VlkError.hpp"
#include "VlkImage.hpp"

VlkImage::VlkImage( VmaAllocator allocator, const VkImageCreateInfo& createInfo )
    : m_allocator( allocator )
{
    ZoneScoped;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

    VkVerify( vmaCreateImage( m_allocator, &createInfo, &allocInfo, &m_image, &m_allocation, nullptr ) );
}

VlkImage::~VlkImage()
{
    ZoneScoped;
    vmaDestroyImage( m_allocator, m_image, m_allocation );
}
