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

    operator VkImage() const { return *m_image; }
    operator VkImageView() const { return *m_imageView; }

private:
    void WriteBarrier( VkCommandBuffer cmdbuf );
    void ReadBarrier( VkCommandBuffer cmdbuf );
    void ReadBarrierTrn( VkCommandBuffer cmdbuf, uint32_t trnQueue, uint32_t gfxQueue );
    void ReadBarrierGfx( VkCommandBuffer cmdbuf, uint32_t trnQueue, uint32_t gfxQueue );

    std::shared_ptr<VlkImage> m_image;
    std::unique_ptr<VlkImageView> m_imageView;
};
