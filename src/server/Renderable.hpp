#pragma once

#include <vulkan/vulkan.h>

class Connector;

class Renderable
{
public:
    virtual ~Renderable() = default;

    virtual void Render( Connector& connector, VkCommandBuffer cmdBuf ) = 0;

    virtual void AddConnector( Connector& connector ) = 0;
    virtual void RemoveConnector( Connector& connector ) = 0;
};
