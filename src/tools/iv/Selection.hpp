#pragma once

#include <memory>
#include <vulkan/vulkan.h>

class GarbageChute;
class VlkBuffer;
class VlkCommandBuffer;
class VlkDevice;
class VlkPipeline;
class VlkPipelineLayout;
class VlkShader;

class Selection
{
public:
    Selection( GarbageChute& garbage, std::shared_ptr<VlkDevice> device, VkFormat format, float scale );
    ~Selection();

    void Update( float delta );
    void Render( VlkCommandBuffer& cmdbuf, const VkExtent2D& extent );
    void SetScale( float scale );
    void FormatChange( VkFormat format );

    void Unselect() { m_active = false; }

    [[nodiscard]] bool IsActive() const { return m_active; }

private:
    void CreatePipeline( VkFormat format );

    GarbageChute& m_garbage;
    std::shared_ptr<VlkDevice> m_device;

    std::shared_ptr<VlkShader> m_shader;
    std::shared_ptr<VlkShader> m_shaderPq;
    std::shared_ptr<VlkPipelineLayout> m_pipelineLayout;
    std::shared_ptr<VlkPipeline> m_pipeline;
    std::shared_ptr<VlkBuffer> m_vertexBuffer;
    std::shared_ptr<VlkBuffer> m_indexBuffer;

    float m_div;
    float m_offset = 0;

    bool m_active;
};
