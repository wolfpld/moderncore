#pragma once

#include <vulkan/vulkan.h>

class Connector;

class Renderable
{
public:
    virtual ~Renderable() = default;

    virtual void Render( Connector& connector, VkCommandBuffer cmdBuf ) = 0;
};
