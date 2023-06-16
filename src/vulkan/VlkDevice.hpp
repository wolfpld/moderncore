#ifndef __VLKDEVICE_HPP__
#define __VLKDEVICE_HPP__

#include <array>
#include <memory>
#include <vulkan/vulkan.h>

#include "VlkDescriptorPool.hpp"
#include "VlkQueueType.hpp"
#include "../../contrib/vk_mem_alloc.h"

class VlkCommandBuffer;
class VlkCommandPool;
class VlkDescriptorPool;

class VlkDevice
{
public:
    enum Flags
    {
        RequireGraphic  = 1 << 0,
        RequireCompute  = 1 << 1,
        RequireTransfer = 1 << 2,
        RequirePresent  = 1 << 3,
    };

    struct QueueInfo
    {
        int idx = -1;
        bool shareGraphic;
        bool shareCompute;
        bool shareTransfer;
        bool sharePresent;
    };

    VlkDevice( VkInstance instance, VkPhysicalDevice physDev, int flags, VkSurfaceKHR presentSurface = VK_NULL_HANDLE );
    ~VlkDevice();

    void Submit( const VlkCommandBuffer& cmdbuf, VkFence fence );

    [[nodiscard]] const QueueInfo& GetQueueInfo( QueueType type ) const { return m_queueInfo[(int)type]; }
    [[nodiscard]] VkQueue GetQueue( QueueType type ) const { return m_queue[(int)type]; }
    [[nodiscard]] const std::shared_ptr<VlkCommandPool>& GetCommandPool( QueueType type ) const { return m_commandPool[(int)type]; }

    operator VkDevice() const { return m_device; }
    operator VmaAllocator() const { return m_allocator; }
    operator VkDescriptorPool() const { return *m_descriptorPool; }

private:
    VkDevice m_device;
    std::array<QueueInfo, 4> m_queueInfo;
    std::array<VkQueue, 4> m_queue;

    VmaAllocator m_allocator;

    std::array<std::shared_ptr<VlkCommandPool>, 4> m_commandPool;

    std::unique_ptr<VlkDescriptorPool> m_descriptorPool;
};

#endif
