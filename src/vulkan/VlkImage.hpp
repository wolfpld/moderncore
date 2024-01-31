#pragma once

#include <vulkan/vulkan.h>

#include "contrib/vk_mem_alloc.h"
#include "util/NoCopy.hpp"

class VlkImage
{
public:
    VlkImage( VmaAllocator allocator, const VkImageCreateInfo& createInfo );
    ~VlkImage();

    NoCopy( VlkImage );

    operator VkImage() const { return m_image; }

private:
    VkImage m_image;
    VmaAllocation m_allocation;
    VmaAllocator m_allocator;
};
