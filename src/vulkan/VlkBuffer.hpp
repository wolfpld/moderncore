#pragma once

#include <vulkan/vulkan.h>

#include "../../contrib/vk_mem_alloc.h"
#include "../util/NoCopy.hpp"

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

    NoCopy( VlkBuffer );

    void Flush( VkDeviceSize offset, VkDeviceSize size );
    void Invalidate( VkDeviceSize offset, VkDeviceSize size );

    void Flush() { Flush( 0, m_size ); }
    void Invalidate() { Invalidate( 0, m_size ); }

    [[nodiscard]] void* Ptr() const { return m_map; }

    operator VkBuffer() const { return m_buffer; }

private:
    VkBuffer m_buffer;
    VmaAllocation m_alloc;
    VmaAllocator m_allocator;

    void* m_map;
    VkDeviceSize m_size;
};
