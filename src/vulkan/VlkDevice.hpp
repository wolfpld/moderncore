#pragma once

#include <array>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <vulkan/vulkan.h>
#include <tracy/TracyVulkan.hpp>
#include <vk_mem_alloc.h>

#include "VlkPhysicalDevice.hpp"
#include "VlkQueueType.hpp"
#include "util/NoCopy.hpp"
#include "util/Panic.hpp"

class VlkCommandBuffer;
class VlkCommandPool;
class VlkGarbage;
class VlkInstance;

class VlkDevice
{
public:
    enum Flags
    {
        RequireGraphic  = 1 << 0,
        RequireCompute  = 1 << 1,
        RequirePresent  = 1 << 2,
    };

    struct QueueInfo
    {
        int idx = -1;
        bool shareGraphic;
        bool shareCompute;
        bool shareTransfer;
        bool sharePresent;
    };

    struct DeviceException : public std::runtime_error { explicit DeviceException( const std::string& msg ) : std::runtime_error( msg ) {} };

    VlkDevice( VlkInstance& instance, std::shared_ptr<VlkPhysicalDevice> physDev, int flags, VkSurfaceKHR presentSurface = VK_NULL_HANDLE );
    ~VlkDevice();

    NoCopy( VlkDevice );

    void Submit( const VlkCommandBuffer& cmdbuf, VkFence fence );

    [[nodiscard]] auto& GetQueueInfo( QueueType type ) const { return m_queueInfo[(int)type]; }
    [[nodiscard]] auto GetQueue( QueueType type ) const { CheckPanic( m_queue[(int)type] != VK_NULL_HANDLE, "Queue does not exist" ); return m_queue[(int)type]; }
    [[nodiscard]] auto& GetCommandPool( QueueType type ) const { CheckPanic( m_commandPool[(int)type], "Command pool does not exist" ); return m_commandPool[(int)type]; }
    [[nodiscard]] auto& GetPhysicalDevice() const { return m_physDev; }
    [[nodiscard]] auto& GetGarbage() const { return m_garbage; }

    [[nodiscard]] bool UseHostImageCopy() const { return m_hostImageCopy; }

    operator VkDevice() const { return m_device; }
    operator VkPhysicalDevice() const { return *m_physDev; }
    operator VmaAllocator() const { return m_allocator; }

#ifdef TRACY_ENABLE
    [[nodiscard]] auto& GetTracyContext() const { return m_tracyCtx; }
#endif

    void lock( QueueType type ) { CheckPanic( m_queueLock[(int)type], "Queue lock does not exist" ); m_queueLock[(int)type]->lock(); }
    void unlock( QueueType type ) { CheckPanic( m_queueLock[(int)type], "Queue lock does not exist" ); m_queueLock[(int)type]->unlock(); }

private:
    VkDevice m_device;
    std::shared_ptr<VlkPhysicalDevice> m_physDev;

    VmaAllocator m_allocator;

    std::array<QueueInfo, 4> m_queueInfo;
    std::array<VkQueue, 4> m_queue;
    std::array<std::shared_ptr<VlkCommandPool>, 4> m_commandPool;
    std::array<std::shared_ptr<std::mutex>, 4> m_queueLock;

    bool m_hostImageCopy;

#ifdef TRACY_ENABLE
    TracyVkCtx m_tracyCtx = nullptr;
#endif

    std::shared_ptr<VlkGarbage> m_garbage;
};
