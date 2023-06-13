#ifndef __VLKBUFFER_HPP__
#define __VLKBUFFER_HPP__

#include <vulkan/vulkan.h>

#include "../../contrib/vk_mem_alloc.h"

class VlkBuffer
{
public:
    enum Flags
    {
        PreferDevice    = 1 << 0,
        PreferHost      = 1 << 1,
        WillWrite       = 1 << 2,
        WillRead        = 1 << 3
    };

    VlkBuffer( VmaAllocator allocator, const VkBufferCreateInfo& createInfo, int flags = 0 );
    ~VlkBuffer();

    void Flush( VkDeviceSize offset, VkDeviceSize size );
    void Invalidate( VkDeviceSize offset, VkDeviceSize size );

    [[nodiscard]] void* Ptr() const { return m_map; }

    operator VkBuffer() const { return m_buffer; }

private:
    VkBuffer m_buffer;
    VmaAllocation m_alloc;
    VmaAllocator m_allocator;

    void* m_map;
};

#endif
