#ifndef __VLKCOMMANDBUFFER_HPP__
#define __VLKCOMMANDBUFFER_HPP__

#include <vulkan/vulkan.h>

class VlkCommandPool;

class VlkCommandBuffer
{
public:
    explicit VlkCommandBuffer( const VlkCommandPool& pool, bool primary = true );
    ~VlkCommandBuffer();

    void Begin( VkCommandBufferUsageFlags flags = 0 );
    void End();

    void Reset( VkCommandBufferResetFlags flags = 0 );

    operator VkCommandBuffer() const { return m_commandBuffer; }

private:
    VkCommandBuffer m_commandBuffer;
    VkCommandPool m_commandPool;
    VkDevice m_device;
};

#endif
