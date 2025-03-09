#pragma once

#include <array>
#include <memory>
#include <mutex>
#include <vulkan/vulkan.h>

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

public:
    ImageView( GarbageChute& garbage, std::shared_ptr<VlkDevice> device, VkFormat format, const VkExtent2D& extent, float scale );
    ~ImageView();

    void Render( VlkCommandBuffer& cmdbuf, const VkExtent2D& extent );
    void Resize( const VkExtent2D& extent );

    void SetBitmap( const std::shared_ptr<Bitmap>& bitmap, TaskDispatch& td );
    void SetBitmap( const std::shared_ptr<BitmapHdr>& bitmap, TaskDispatch& td );
    void SetScale( float scale ) { m_div = 1.f / 8.f / scale; }
    void FormatChange( VkFormat format );

    [[nodiscard]] bool HasBitmap();

private:
    void CreatePipeline( VkFormat format );

    void Cleanup();
    [[nodiscard]] std::array<Vertex, 4> SetupVertexBuffer() const;
    void FinishSetBitmap( std::shared_ptr<Texture>&& texture, std::shared_ptr<VlkBuffer>&& vb, uint32_t width, uint32_t height );

    GarbageChute& m_garbage;
    std::shared_ptr<VlkDevice> m_device;

    std::shared_ptr<VlkShader> m_shader;
    std::shared_ptr<VlkShader> m_shaderPq;
    std::shared_ptr<VlkDescriptorSetLayout> m_setLayout;
    std::shared_ptr<VlkPipelineLayout> m_pipelineLayout;
    std::shared_ptr<VlkPipeline> m_pipeline;
    std::shared_ptr<VlkBuffer> m_vertexBuffer;
    std::shared_ptr<VlkBuffer> m_indexBuffer;
    std::shared_ptr<Texture> m_texture;
    std::shared_ptr<VlkSampler> m_sampler;

    VkExtent2D m_extent;
    VkExtent2D m_bitmapExtent;

    VkDescriptorImageInfo m_imageInfo;
    VkWriteDescriptorSet m_descWrite;

    float m_div;

    std::mutex m_lock;
};
