#ifndef __VLKIMAGEVIEW_HPP__
#define __VLKIMAGEVIEW_HPP__

#include <vulkan/vulkan.h>

class VlkImageView
{
public:
    VlkImageView( VkDevice device, const VkImageViewCreateInfo& createInfo );
    ~VlkImageView();

    operator VkImageView() const { return m_imageView; }

private:
    VkImageView m_imageView;
    VkDevice m_device;
};

#endif
