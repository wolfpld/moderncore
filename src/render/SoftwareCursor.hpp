#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <vulkan/vulkan.h>

#include "util/NoCopy.hpp"

class Bitmap;
struct CursorBitmap;
class CursorLogic;
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

    struct ScreenUniform
    {
        glm::vec2 screenSize;
    };

public:
    explicit SoftwareCursor( VlkDevice& device, VkRenderPass renderPass, uint32_t screenWidth, uint32_t screenHeight );
    ~SoftwareCursor();

    NoCopy( SoftwareCursor );

    void SetPosition( float x, float y );
    void Render( VkCommandBuffer cmdBuf, CursorLogic& cursorLogic );

private:
    void UpdateVertexBuffer( uint32_t width, uint32_t height );
    void UpdateImage( const CursorBitmap& cursorData );

    float m_x, m_y;
    uint32_t m_w, m_h;
    uint32_t m_xhot, m_yhot;
    Bitmap* m_currentBitmap;

    std::unique_ptr<VlkShader> m_shader;
    std::unique_ptr<VlkDescriptorSetLayout> m_descriptorSetLayout;
    std::unique_ptr<VlkPipelineLayout> m_pipelineLayout;
    std::unique_ptr<VlkPipeline> m_pipeline;
    std::unique_ptr<VlkBuffer> m_vertexBuffer;
    std::unique_ptr<VlkBuffer> m_indexBuffer;
    std::unique_ptr<VlkBuffer> m_uniformBuffer;
    std::unique_ptr<Texture> m_image;
    std::unique_ptr<VlkSampler> m_sampler;

    VlkDevice& m_device;
    VkDescriptorImageInfo m_imageInfo;
    VkDescriptorBufferInfo m_uniformInfo;
    VkWriteDescriptorSet m_descriptorWrite[2];
};
