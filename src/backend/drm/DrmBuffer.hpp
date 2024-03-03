#pragma once

#include <vector>
#include <vulkan/vulkan.h>
#include <xf86drmMode.h>

class DrmDevice;
struct gbm_bo;

class DrmBuffer
{
public:
    DrmBuffer( DrmDevice& device, const drmModeModeInfo& mode, const std::vector<uint64_t>& modifiers );
    ~DrmBuffer();

private:
    int FindMemoryType( uint32_t typeBits, VkMemoryPropertyFlags properties );

    DrmDevice& m_device;

    gbm_bo* m_bo;
    uint64_t m_modifier;

    VkImage m_image;
};
