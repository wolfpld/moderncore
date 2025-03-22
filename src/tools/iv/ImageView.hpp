#pragma once

#include <array>
#include <memory>
#include <mutex>
#include <vulkan/vulkan.h>

#include "util/Vector2.hpp"

class Bitmap;
class BitmapHdr;
class GarbageChute;
class TaskDispatch;
class Texture;
class VlkBuffer;
class VlkCommandBuffer;
class VlkDescriptorSetLayout;
class VlkDevice;
class VlkPipeline;
class VlkPipelineLayout;
class VlkSampler;
class VlkShader;

class ImageView
{
    struct Vertex
    {
        float x, y;
        float u, v;
    };

    enum class FitMode
    {
        None,
        TooSmall,
        Always
    };

public:
    ImageView( GarbageChute& garbage, std::shared_ptr<VlkDevice> device, VkFormat format, const VkExtent2D& extent, float scale );
    ~ImageView();

    void Render( VlkCommandBuffer& cmdbuf, const VkExtent2D& extent );
    void Resize( const VkExtent2D& extent );

    void SetBitmap( const std::shared_ptr<Bitmap>& bitmap, TaskDispatch& td );
    void SetBitmap( const std::shared_ptr<BitmapHdr>& bitmap, TaskDispatch& td );
    void SetScale( float scale, const VkExtent2D& extent );
    void FormatChange( VkFormat format );

    void FitToExtent( const VkExtent2D& extent );
    void FitToWindow( const VkExtent2D& extent );
    void FitPixelPerfect( const VkExtent2D& extent, uint32_t zoom, const Vector2<float>* focus = nullptr );

    void Pan( const Vector2<float>& delta );
    void Zoom( const Vector2<float>& focus, float factor );

    [[nodiscard]] bool HasBitmap();
    [[nodiscard]] VkExtent2D GetBitmapExtent() const { return m_bitmapExtent; }
    [[nodiscard]] float GetImgScale() const { return m_imgScale; }

private:
    void CreatePipeline( VkFormat format );

    void Cleanup();
    [[nodiscard]] std::array<Vertex, 4> SetupVertexBuffer() const;
    void UpdateVertexBuffer();
    void FinishSetBitmap( std::shared_ptr<Texture>&& texture, std::shared_ptr<VlkBuffer>&& vb, uint32_t width, uint32_t height );

    void ClampImagePosition();
    void SetImgScale( float scale );

    void FitToExtentUnlocked( const VkExtent2D& extent );
    void FitToWindowUnlocked( const VkExtent2D& extent );

    GarbageChute& m_garbage;
    std::shared_ptr<VlkDevice> m_device;

    std::shared_ptr<VlkShader> m_shaderMin[2];
    std::shared_ptr<VlkShader> m_shaderExact[2];
    std::shared_ptr<VlkShader> m_shaderNearest[2];
    std::shared_ptr<VlkDescriptorSetLayout> m_setLayout;
    std::shared_ptr<VlkPipelineLayout> m_pipelineLayout;
    std::shared_ptr<VlkPipeline> m_pipelineMin;
    std::shared_ptr<VlkPipeline> m_pipelineExact;
    std::shared_ptr<VlkPipeline> m_pipelineNearest;
    std::shared_ptr<VlkBuffer> m_vertexBuffer;
    std::shared_ptr<VlkBuffer> m_indexBuffer;
    std::shared_ptr<Texture> m_texture;
    std::shared_ptr<VlkSampler> m_samplerLinear;
    std::shared_ptr<VlkSampler> m_samplerNearest;

    VkExtent2D m_extent;
    VkExtent2D m_bitmapExtent;

    Vector2<float> m_imgOrigin;
    float m_imgScale;
    bool m_filteredNearest;

    VkDescriptorImageInfo m_imageInfo;
    VkWriteDescriptorSet m_descWrite;

    float m_div;
    float m_scale;
    FitMode m_fitMode;

    std::mutex m_lock;
};
