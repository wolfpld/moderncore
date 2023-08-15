#pragma once

#include <memory>
#include <vulkan/vulkan.h>

#include "../util/NoCopy.hpp"
#include "../vulkan/VlkImage.hpp"
#include "../vulkan/VlkImageView.hpp"

class Bitmap;
class VlkDevice;

class Texture
{
public:
    Texture( VlkDevice& device, const Bitmap& bitmap, VkFormat format );

    NoCopy( Texture );

    operator VkImageView() const { return *m_imageView; }

private:
    std::unique_ptr<VlkImage> m_image;
    std::unique_ptr<VlkImageView> m_imageView;
};
