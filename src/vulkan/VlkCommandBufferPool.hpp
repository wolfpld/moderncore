#pragma once

#include <stdint.h>
#include <vector>
#include <vulkan/vulkan.h>

#include "util/NoCopy.hpp"

class VlkCommandPool;

class VlkCommandBufferPool
{
public:
    VlkCommandBufferPool( const VlkCommandPool& pool, uint32_t initialCount );
    ~VlkCommandBufferPool();

    NoCopy( VlkCommandBufferPool );

    [[nodiscard]] VkCommandBuffer Acquire();
    void End( VkCommandBuffer cmdBuf );
    void Release( VkCommandBuffer cmdBuf );

private:
    [[nodiscard]] VkCommandBuffer Allocate();

    const VlkCommandPool& m_pool;
    std::vector<VkCommandBuffer> m_all;
    std::vector<VkCommandBuffer> m_free;
};
