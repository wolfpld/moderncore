#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include "VlkBase.hpp"
#include "util/NoCopy.hpp"

class VlkImage : public VlkBase
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
