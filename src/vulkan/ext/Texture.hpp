#pragma once

#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

#include "util/NoCopy.hpp"
#include "vulkan/VlkBase.hpp"
#include "vulkan/VlkImage.hpp"
#include "vulkan/VlkImageView.hpp"

class Bitmap;
class TaskDispatch;
class VlkDevice;
class VlkFence;

class Texture : public VlkBase
{
public:
    Texture( VlkDevice& device, const Bitmap& bitmap, VkFormat format, bool mips, std::vector<std::shared_ptr<VlkFence>>& fencesOut, TaskDispatch* td = nullptr );

    NoCopy( Texture );

    operator VkImage() const { return *m_image; }
    operator VkImageView() const { return *m_imageView; }

private:
    void WriteBarrier( VkCommandBuffer cmdbuf, uint32_t mip );
    void ReadBarrier( VkCommandBuffer cmdbuf, uint32_t mipLevels );
    void ReadBarrierTrn( VkCommandBuffer cmdbuf, uint32_t mipLevels, uint32_t trnQueue, uint32_t gfxQueue );
    void ReadBarrierGfx( VkCommandBuffer cmdbuf, uint32_t mipLevels, uint32_t trnQueue, uint32_t gfxQueue );

    std::shared_ptr<VlkImage> m_image;
    std::unique_ptr<VlkImageView> m_imageView;
};
