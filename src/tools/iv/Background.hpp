#pragma once

#include <memory>

class GarbageChute;
class VlkBuffer;
class VlkDevice;
class VlkPipeline;
class VlkShader;

class Background
{
public:
    Background( GarbageChute& garbage, std::shared_ptr<VlkDevice> device );
    ~Background();

private:
    GarbageChute& m_garbage;
    std::shared_ptr<VlkDevice> m_device;

    std::shared_ptr<VlkShader> m_shader;
    std::shared_ptr<VlkPipeline> m_pipeline;
    std::shared_ptr<VlkBuffer> m_vertexBuffer;
};
