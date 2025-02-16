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

class Background
{
public:
    Background( GarbageChute& garbage, std::shared_ptr<VlkDevice> device );
    ~Background();

    void Render( VlkCommandBuffer& cmdbuf, VkExtent2D extent );

private:
    GarbageChute& m_garbage;
    std::shared_ptr<VlkDevice> m_device;

    std::shared_ptr<VlkShader> m_shader;
    std::shared_ptr<VlkPipelineLayout> m_pipelineLayout;
    std::shared_ptr<VlkPipeline> m_pipeline;
    std::shared_ptr<VlkBuffer> m_vertexBuffer;
};
