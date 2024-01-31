#pragma once

#include <vulkan/vulkan.h>

#include "util/NoCopy.hpp"

class VlkImageView
{
public:
    VlkImageView( VkDevice device, const VkImageViewCreateInfo& createInfo );
    ~VlkImageView();

    NoCopy( VlkImageView );

    operator VkImageView() const { return m_imageView; }

private:
    VkImageView m_imageView;
    VkDevice m_device;
};
