#ifndef __TEXTURE_HPP__
#define __TEXTURE_HPP__

#include <memory>

#include "../util/NoCopy.hpp"
#include "../vulkan/VlkImage.hpp"
#include "../vulkan/VlkImageView.hpp"

class Bitmap;
class VlkDevice;

class Texture
{
public:
    Texture( VlkDevice& device, const Bitmap& bitmap );

    NoCopy( Texture );

    operator VkImageView() const { return *m_imageView; }

private:
    std::unique_ptr<VlkImage> m_image;
    std::unique_ptr<VlkImageView> m_imageView;
};

#endif
