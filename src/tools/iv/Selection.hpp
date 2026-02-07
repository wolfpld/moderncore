#pragma once

#include <memory>
#include <vulkan/vulkan.h>

class GarbageChute;
class VlkDevice;

class Selection
{
public:
    Selection( GarbageChute& garbage, std::shared_ptr<VlkDevice> device, VkFormat format );
    ~Selection();

private:
    GarbageChute& m_garbage;
    std::shared_ptr<VlkDevice> m_device;
};
