#ifndef __VLKDEVICE_HPP__
#define __VLKDEVICE_HPP__

#include <vulkan/vulkan.h>

#include "../../contrib/vk_mem_alloc.h"

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

    enum class QueueType
    {
        Graphic,
        Compute,
        Transfer,
        Present
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

    void Submit( QueueType type, VkCommandBuffer cmdbuf, VkFence fence );

    [[nodiscard]] const QueueInfo& GetQueueInfo( QueueType type ) const { return m_queueInfo[(int)type]; }
    [[nodiscard]] VkQueue GetQueue( QueueType type ) const { return m_queue[(int)type]; }

    operator VkDevice() const { return m_device; }
    operator VmaAllocator() const { return m_allocator; }

private:
    VkDevice m_device;
    QueueInfo m_queueInfo[4];
    VkQueue m_queue[4];

    VmaAllocator m_allocator;
};

#endif
