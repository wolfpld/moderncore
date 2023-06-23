#ifndef __SOFTWARECURSOR_HPP__
#define __SOFTWARECURSOR_HPP__

#include <glm/glm.hpp>
#include <memory>
#include <vulkan/vulkan.h>

class Texture;
class VlkBuffer;
class VlkDescriptorSetLayout;
class VlkDevice;
class VlkPipeline;
class VlkPipelineLayout;
class VlkSampler;
class VlkShader;

class SoftwareCursor
{
    struct Vertex
    {
        glm::vec2 pos;
        glm::vec2 uv;
    };

    struct Position
    {
        glm::vec2 pos;
    };

public:
    explicit SoftwareCursor( VlkDevice& device, VkRenderPass renderPass, uint32_t screenWidth, uint32_t screenHeight );
    ~SoftwareCursor();

    void SetPosition( float x, float y );
    void Render( VkCommandBuffer cmdBuf );

private:
    Position m_position;

    uint32_t m_w, m_h;

    std::unique_ptr<VlkShader> m_shader;
    std::unique_ptr<VlkDescriptorSetLayout> m_descriptorSetLayout;
    std::unique_ptr<VlkPipelineLayout> m_pipelineLayout;
    std::unique_ptr<VlkPipeline> m_pipeline;
    std::unique_ptr<VlkBuffer> m_vertexBuffer;
    std::unique_ptr<VlkBuffer> m_indexBuffer;
    std::unique_ptr<Texture> m_image;
    std::unique_ptr<VlkSampler> m_sampler;

    VkDescriptorImageInfo m_imageInfo;
    VkWriteDescriptorSet m_descriptorWrite;
};

#endif
