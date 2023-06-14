#ifndef __SOFTWARECURSOR_HPP__
#define __SOFTWARECURSOR_HPP__

#include <glm/glm.hpp>
#include <memory>
#include <vulkan/vulkan.h>

class VlkBuffer;
class VlkDevice;
class VlkPipeline;
class VlkPipelineLayout;
class VlkShader;

class SoftwareCursor
{
    struct Vertex
    {
        glm::vec2 pos;
        glm::vec2 uv;
    };

public:
    explicit SoftwareCursor( const VlkDevice& device, VkRenderPass renderPass, uint32_t screenWidth, uint32_t screenHeight );
    ~SoftwareCursor();

    void SetPosition( float x, float y );
    void Render( VkCommandBuffer cmdBuf );

private:
    double m_x = 0;
    double m_y = 0;

    std::unique_ptr<VlkShader> m_shader;
    std::unique_ptr<VlkPipelineLayout> m_pipelineLayout;
    std::unique_ptr<VlkPipeline> m_pipeline;
    std::unique_ptr<VlkBuffer> m_vertexBuffer;
    std::unique_ptr<VlkBuffer> m_indexBuffer;
};

#endif
