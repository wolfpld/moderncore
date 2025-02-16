#pragma once

#include <memory>

class VlkBuffer;
class VlkPipeline;
class VlkShader;

class Background
{
public:
    Background();
    ~Background();

private:
    std::shared_ptr<VlkShader> m_shader;
    std::shared_ptr<VlkPipeline> m_pipeline;
    std::shared_ptr<VlkBuffer> m_vertexBuffer;
};
