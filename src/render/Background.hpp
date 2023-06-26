#ifndef __BACKGROUND_HPP__
#define __BACKGROUND_HPP__

#include <glm/glm.hpp>
#include <memory>
#include <stdint.h>
#include <vulkan/vulkan.h>

#include "../util/NoCopy.hpp"

class Bitmap;
class Texture;
class VlkBuffer;
class VlkDescriptorSetLayout;
class VlkDevice;
class VlkPipeline;
class VlkPipelineLayout;
class VlkSampler;
class VlkShader;

class Background
{
    struct Vertex
    {
        glm::vec2 pos;
        glm::vec2 uv;
    };

public:
    Background( VlkDevice& device, VkRenderPass renderPass, uint32_t screenWidth, uint32_t screenHeight );
    ~Background();

    NoCopy( Background );

    void Render( VkCommandBuffer cmdBuf );

    [[nodiscard]] const VkClearValue& GetColor() const { return m_color; }

private:
    void UpdateVertexBuffer();

    VkClearValue m_color;

    std::unique_ptr<Bitmap> m_bitmap;
    std::unique_ptr<Texture> m_texture;

    std::unique_ptr<VlkShader> m_shader;
    std::unique_ptr<VlkDescriptorSetLayout> m_descriptorSetLayout;
    std::unique_ptr<VlkPipelineLayout> m_pipelineLayout;
    std::unique_ptr<VlkPipeline> m_pipeline;
    std::unique_ptr<VlkBuffer> m_vertexBuffer;
    std::unique_ptr<VlkBuffer> m_indexBuffer;
    std::unique_ptr<VlkSampler> m_sampler;

    VkDescriptorImageInfo m_imageInfo;
    VkWriteDescriptorSet m_descriptorWrite;
};

#endif
