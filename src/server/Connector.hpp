#pragma once

#include <memory>
#include <stdint.h>

#include "util/NoCopy.hpp"
#include "vulkan/VlkRenderPass.hpp"

class Renderable;
class VlkDevice;

class Connector
{
public:
    explicit Connector( VlkDevice& device ) : m_device( device ) {}
    virtual ~Connector() = default;

    NoCopy( Connector );

    virtual void Render() = 0;

    [[nodiscard]] auto& Device() const { return m_device; }
    [[nodiscard]] auto Width() const { return m_width; }
    [[nodiscard]] auto Height() const { return m_height; }
    [[nodiscard]] auto& RenderPass() const { return *m_renderPass; }

protected:
    VlkDevice& m_device;

    uint32_t m_width = 0;
    uint32_t m_height = 0;

    std::unique_ptr<VlkRenderPass> m_renderPass;
};
