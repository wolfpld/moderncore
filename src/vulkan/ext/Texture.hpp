#pragma once

#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

#include "util/NoCopy.hpp"
#include "vulkan/VlkBase.hpp"
#include "vulkan/VlkImage.hpp"
#include "vulkan/VlkImageView.hpp"

class Bitmap;
class BitmapHdr;
class BitmapHdrHalf;
class MipGen;
struct MipData;
class TaskDispatch;
class VlkDevice;
class VlkFence;

class Texture : public VlkBase
{
public:
    Texture( VlkDevice& device, const Bitmap& bitmap, VkFormat format, bool mips, std::vector<std::shared_ptr<VlkFence>>& fencesOut, TaskDispatch* td = nullptr, MipGen* mipGen = nullptr );
    Texture( VlkDevice& device, const BitmapHdr& bitmap, VkFormat format, bool mips, std::vector<std::shared_ptr<VlkFence>>& fencesOut, TaskDispatch* td = nullptr, MipGen* mipGen = nullptr );
    NoCopy( Texture );

    std::shared_ptr<Bitmap> ReadbackSdr( VlkDevice& device ) const;
    std::shared_ptr<BitmapHdrHalf> ReadbackHdr( VlkDevice& device ) const;

    [[nodiscard]] VkFormat Format() const { return m_format; }

    operator VkImage() const { return *m_image; }
    operator VkImageView() const { return *m_imageView; }

private:
    template<typename T>
    void Upload( VlkDevice& device, const std::vector<MipData>& mipChain, uint32_t bpp, const T* bmpptr, std::unique_ptr<T>&& tmp, TaskDispatch* td, std::vector<std::shared_ptr<VlkFence>>& fencesOut, MipGen* mipGen );
    void FinishUpload( VlkDevice& device, uint32_t mipLevels, std::vector<std::shared_ptr<VlkFence>>& fencesOut );
    void FinishUploadGpu( VlkDevice& device, MipGen& mipGen, uint32_t mipLevels, std::vector<std::shared_ptr<VlkFence>>& fencesOut, bool hostCopied );

    void WriteBarrier( VkCommandBuffer cmdbuf, uint32_t mip );
    void ReadBarrier( VkCommandBuffer cmdbuf, uint32_t mipLevels );
    void ReadBarrierTx( VkCommandBuffer cmdbuf, uint32_t mipLevels, uint32_t trnQueue, uint32_t gfxQueue );
    void ReadBarrierGfx( VkCommandBuffer cmdbuf, uint32_t mipLevels, uint32_t trnQueue, uint32_t gfxQueue );

    std::shared_ptr<VlkImage> m_image;
    std::unique_ptr<VlkImageView> m_imageView;

    VkFormat m_format;
    uint32_t m_width, m_height;
};
