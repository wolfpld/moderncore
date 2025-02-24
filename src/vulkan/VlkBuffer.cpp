#include <tracy/Tracy.hpp>

#include "VlkBuffer.hpp"
#include "VlkError.hpp"

VlkBuffer::VlkBuffer( VmaAllocator allocator, const VkBufferCreateInfo& createInfo, int flags )
    : m_allocator( allocator )
    , m_size( createInfo.size )
{
    ZoneScoped;

    VmaAllocationCreateInfo allocInfo = {};

    if( flags & PreferDevice ) allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    else if( flags & PreferHost ) allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
    else allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

    allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    if( flags & WillWrite ) allocInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    if( flags & WillRead ) allocInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;

    VmaAllocationInfo info;
    VkVerify( vmaCreateBuffer( allocator, &createInfo, &allocInfo, &m_buffer, &m_alloc, &info ) );

    m_map = info.pMappedData;
}

VlkBuffer::~VlkBuffer()
{
    ZoneScoped;
    vmaDestroyBuffer( m_allocator, m_buffer, m_alloc );
}

void VlkBuffer::Flush( VkDeviceSize offset, VkDeviceSize size )
{
    ZoneScoped;
    vmaFlushAllocation( m_allocator, m_alloc, offset, size );
}

void VlkBuffer::Invalidate( VkDeviceSize offset, VkDeviceSize size )
{
    ZoneScoped;
    vmaInvalidateAllocation( m_allocator, m_alloc, offset, size );
}
