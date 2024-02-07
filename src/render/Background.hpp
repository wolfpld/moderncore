#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <stdint.h>
#include <vulkan/vulkan.h>

#include "server/Renderable.hpp"
#include "util/FileBuffer.hpp"
#include "util/NoCopy.hpp"
#include "util/RobinHood.hpp"

class Bitmap;
class Connector;
class Texture;
class VlkBuffer;
class VlkDescriptorSetLayout;
class VlkDevice;
class VlkPipeline;
class VlkPipelineLayout;
class VlkSampler;
class VlkShader;

class Background : public Renderable
{
    struct Vertex
    {
        glm::vec2 pos;
        glm::vec2 uv;
    };

    struct DrawData
    {
        std::unique_ptr<Texture> texture;
        std::unique_ptr<VlkShader> shader;
        std::unique_ptr<VlkDescriptorSetLayout> descriptorSetLayout;
        std::unique_ptr<VlkPipelineLayout> pipelineLayout;
        std::unique_ptr<VlkPipeline> pipeline;
        std::unique_ptr<VlkBuffer> vertexBuffer;
        std::unique_ptr<VlkBuffer> indexBuffer;
        std::unique_ptr<VlkSampler> sampler;

        VkDescriptorImageInfo imageInfo;
        VkWriteDescriptorSet descriptorWrite;
    };

public:
    Background();
    ~Background() override;

    NoCopy( Background );

    void Render( Connector& connector, VkCommandBuffer cmdBuf ) override;

    void AddConnector( Connector& connector ) override;
    void RemoveConnector( Connector& connector ) override;

    [[nodiscard]] const VkClearValue& GetColor() const { return m_color; }

private:
    static void UpdateVertexBuffer( VlkBuffer& buffer ,uint32_t imageWidth, uint32_t imageHeight, uint32_t displayWidth, uint32_t displayHeight );

    VkClearValue m_color;

    FileBuffer m_vert, m_frag;
    std::unique_ptr<Bitmap> m_bitmap;

    unordered_flat_map<Connector*, std::unique_ptr<DrawData>> m_drawData;
};
