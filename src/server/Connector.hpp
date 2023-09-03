#pragma once

#include <memory>
#include <stdint.h>
#include <vector>

#include "../util/NoCopy.hpp"
#include "../vulkan/VlkRenderPass.hpp"

class Renderable;
class VlkDevice;

class Connector
{
public:
    explicit Connector( VlkDevice& device ) : m_device( device ) {}
    virtual ~Connector() = default;

    NoCopy( Connector );

    virtual void Render( const std::vector<std::shared_ptr<Renderable>>& renderables ) = 0;

    [[nodiscard]] VlkDevice& Device() const { return m_device; }
    [[nodiscard]] uint32_t Width() const { return m_width; }
    [[nodiscard]] uint32_t Height() const { return m_height; }
    [[nodiscard]] VlkRenderPass& RenderPass() const { return *m_renderPass; }

protected:
    VlkDevice& m_device;

    uint32_t m_width = 0;
    uint32_t m_height = 0;

    std::unique_ptr<VlkRenderPass> m_renderPass;
};
