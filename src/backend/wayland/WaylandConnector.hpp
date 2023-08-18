#pragma once

#include <memory>
#include <vulkan/vulkan.h>

#include "../../server/Connector.hpp"

class VlkSwapchain;
class VlkDevice;

class WaylandConnector : public Connector
{
public:
    WaylandConnector( const VlkDevice& device, VkSurfaceKHR surface );
    ~WaylandConnector() override;

private:
    std::unique_ptr<VlkSwapchain> m_swapchain;
};
