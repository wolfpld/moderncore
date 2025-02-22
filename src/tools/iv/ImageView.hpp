#pragma once

#include <memory>
#include <mutex>
#include <vulkan/vulkan.h>

class Bitmap;
class GarbageChute;
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
public:
    ImageView( GarbageChute& garbage, std::shared_ptr<VlkDevice> device, VkFormat format );
    ~ImageView();

    void Render( VlkCommandBuffer& cmdbuf, const VkExtent2D& extent );

    void SetBitmap( const std::shared_ptr<Bitmap>& bitmap );

    [[nodiscard]] bool HasBitmap();

private:
    void Cleanup();

    GarbageChute& m_garbage;
    std::shared_ptr<VlkDevice> m_device;

    std::shared_ptr<VlkShader> m_shader;
    std::shared_ptr<VlkDescriptorSetLayout> m_setLayout;
    std::shared_ptr<VlkPipelineLayout> m_pipelineLayout;
    std::shared_ptr<VlkPipeline> m_pipeline;
    std::shared_ptr<VlkBuffer> m_vertexBuffer;
    std::shared_ptr<VlkBuffer> m_indexBuffer;
    std::shared_ptr<Texture> m_texture;
    std::shared_ptr<VlkSampler> m_sampler;

    VkDescriptorImageInfo m_imageInfo;
    VkWriteDescriptorSet m_descWrite;

    std::mutex m_lock;
};
