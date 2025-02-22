#pragma once

#include <memory>
#include <vulkan/vulkan.h>

#include "util/NoCopy.hpp"
#include "vulkan/VlkBase.hpp"
#include "vulkan/VlkImage.hpp"
#include "vulkan/VlkImageView.hpp"

class Bitmap;
class VlkDevice;

class Texture : public VlkBase
{
public:
    Texture( VlkDevice& device, const Bitmap& bitmap, VkFormat format );

    NoCopy( Texture );

    void TransitionLayout( VkCommandBuffer cmdBuf, VkImageLayout layout, VkAccessFlagBits access, VkPipelineStageFlagBits src, VkPipelineStageFlagBits dst );

    operator VkImage() const { return *m_image; }
    operator VkImageView() const { return *m_imageView; }

private:
    std::shared_ptr<VlkImage> m_image;
    std::unique_ptr<VlkImageView> m_imageView;

    VkImageLayout m_layout;
    VkAccessFlagBits m_access;
};
