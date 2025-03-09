#pragma once

#include <memory>
#include <vulkan/vulkan.h>

class GarbageChute;
class SvgImage;
class Texture;
class VlkBuffer;
class VlkCommandBuffer;
class VlkDescriptorSetLayout;
class VlkDevice;
class VlkPipeline;
class VlkPipelineLayout;
class VlkSampler;
class VlkShader;

class BusyIndicator
{
public:
    BusyIndicator( GarbageChute& garbage, std::shared_ptr<VlkDevice> device, VkFormat format, float scale );
    ~BusyIndicator();

    void Update( float delta );
    void ResetTime();
    void Render( VlkCommandBuffer& cmdbuf, const VkExtent2D& extent );
    void SetScale( float scale );
    void FormatChange( VkFormat format );

private:
    void CreatePipeline( VkFormat format );

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

    VkDescriptorImageInfo m_imageInfo;
    VkWriteDescriptorSet m_descWrite;

    float m_scale;
    float m_time = 0;
    float m_rot;

    std::unique_ptr<SvgImage> m_hourglass;
};
