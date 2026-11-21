#pragma once

#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <stdint.h>
#include <vulkan/vulkan.h>

#include "util/NoCopy.hpp"
#include "VlkCommandBufferPool.hpp"

class VlkBuffer;
class VlkDevice;
class VlkFence;

class VlkStagingRing
{
public:
    explicit VlkStagingRing( VlkDevice& device, VkDeviceSize size = 64ull << 20 );
    ~VlkStagingRing();

    NoCopy( VlkStagingRing );

    void Level( VkImage image, uint32_t level, uint32_t width, uint32_t height, uint32_t bpp,
                const void* src, const std::function<void( VkCommandBuffer )>& levelBarrier );

private:
    struct InFlight
    {
        std::shared_ptr<VlkFence> fence;
        VkCommandBuffer cmdBuf;
        uint64_t begin;
        uint64_t end;
    };

    void NeedRegion( uint64_t bytes, uint64_t alignment );
    void RetireOldest();

    std::mutex m_lock;
    VlkDevice& m_device;
    std::shared_ptr<VlkBuffer> m_buffer;
    std::deque<InFlight> m_inFlight;

    VlkCommandBufferPool m_cmds;
    uint64_t m_cursor = 0;
    uint64_t m_size;
    uint64_t m_granularity;
    uint64_t m_atom;
    uint32_t m_transferGranY = 1;
};
