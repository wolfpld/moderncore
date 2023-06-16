#ifndef __VLKIMAGE_HPP__
#define __VLKIMAGE_HPP__

#include <vulkan/vulkan.h>

#include "../../contrib/vk_mem_alloc.h"

class VlkImage
{
public:
    VlkImage( VmaAllocator allocator, const VkImageCreateInfo& createInfo );
    ~VlkImage();

    operator VkImage() const { return m_image; }

private:
    VkImage m_image;
    VmaAllocation m_allocation;
    VmaAllocator m_allocator;
};

#endif
