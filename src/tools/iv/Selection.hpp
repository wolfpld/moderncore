#pragma once

#include <memory>
#include <vulkan/vulkan.h>

#include "util/Vector2.hpp"

class ImageView;
class GarbageChute;
class VlkBuffer;
class VlkCommandBuffer;
class VlkDevice;
class VlkPipeline;
class VlkPipelineLayout;
class VlkShader;
class WaylandWindow;

class Selection
{
public:
    enum class ResizeArea
    {
        None,
        Up,
        Down,
        Left,
        Right,
        UpLeft,
        UpRight,
        DownLeft,
        DownRight
    };

    Selection( std::shared_ptr<WaylandWindow> window, std::shared_ptr<VlkDevice> device, VkFormat format, float scale );
    ~Selection();

    void Update( float delta );
    void Render( VlkCommandBuffer& cmdbuf, const VkExtent2D& extent );
    void SetScale( float scale );
    void FormatChange( VkFormat format );

    void AbortDrag();

    void MouseButton( const Vector2<float>& pos, bool pressed );
    bool MouseMove( const Vector2<float>& pos );

    [[nodiscard]] ResizeArea GetResizeArea( const Vector2<float>& pos ) const;
    [[nodiscard]] ResizeArea ActiveResizeArea() const { return m_resizeArea; }

    [[nodiscard]] bool IsActive() const;
    [[nodiscard]] VkRect2D GetSelection() const;

    void SetImageView( ImageView* imageView );
    void UpdateVertexBuffer();

private:
    void CreatePipeline( VkFormat format );

    [[nodiscard]] Vector2<uint32_t> ScreenToImagePos( const Vector2<float>& pos ) const;
    [[nodiscard]] Vector2<uint32_t> ScreenToImagePosWithOrigin( const Vector2<float>& pos ) const;
    [[nodiscard]] Vector2<uint32_t> ScreenToImagePosRound( const Vector2<float>& pos ) const;
    [[nodiscard]] Vector2<float> ImageToScreenPos( const Vector2<uint32_t>& pos ) const;

    GarbageChute& m_garbage;
    std::shared_ptr<WaylandWindow> m_window;
    std::shared_ptr<VlkDevice> m_device;
    ImageView* m_imageView;

    std::shared_ptr<VlkShader> m_shader;
    std::shared_ptr<VlkShader> m_shaderPq;
    std::shared_ptr<VlkPipelineLayout> m_pipelineLayout;
    std::shared_ptr<VlkPipeline> m_pipeline;
    std::shared_ptr<VlkBuffer> m_vertexBuffer;
    std::shared_ptr<VlkBuffer> m_indexBuffer;

    float m_div;
    float m_offset = 0;

    bool m_drag = false;
    ResizeArea m_resizeArea = ResizeArea::None;

    Vector2<uint32_t> m_posMin, m_posMax, m_origin;
};
