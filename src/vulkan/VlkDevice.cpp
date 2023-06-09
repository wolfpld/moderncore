#include <assert.h>
#include <limits>
#include <vector>

#include "VlkDevice.hpp"
#include "VlkError.hpp"
#include "VlkPhysicalDevice.hpp"
#include "../util/Panic.hpp"

VlkDevice::VlkDevice( VkInstance instance, VkPhysicalDevice physDev, int flags )
    : m_queueInfo {}
    , m_queue {}
{
    CheckPanic( flags & ( RequireGraphic | RequireCompute ), "Requested Device without graphic and compute queues." );

    VlkPhysicalDevice phys( physDev );
    auto& qfp = phys.GetQueueFamilyProperties();
    const auto sz = uint32_t( qfp.size() );

    if( flags & RequireGraphic )
    {
        CheckPanic( phys.IsGraphicCapable(), "Requested Device with graphic queue, but it does not has any graphic queues." );
        uint32_t idx = sz;
        if( flags & RequireCompute )
        {
            for( idx=0; idx<sz; idx++ )
            {
                if(  ( qfp[idx].queueFlags & VK_QUEUE_GRAPHICS_BIT ) &&
                    !( qfp[idx].queueFlags & VK_QUEUE_COMPUTE_BIT  ) )
                {
                    break;
                }
            }
        }
        if( idx == sz )
        {
            for( idx=0; idx<sz; idx++ )
            {
                if( ( qfp[idx].queueFlags & VK_QUEUE_GRAPHICS_BIT ) )
                {
                    break;
                }
            }
        }
        m_queueInfo[(int)QueueType::Graphic].idx = idx;
    }

    if( flags & RequireCompute )
    {
        CheckPanic( phys.IsComputeCapable(), "Requested Device with compute queue, but it does not has any compute queues." );
        uint32_t idx = sz;
        for( idx=0; idx<sz; idx++ )
        {
            if( ( (int)idx != m_queueInfo[(int)QueueType::Graphic].idx ) &&
                ( qfp[idx].queueFlags & VK_QUEUE_COMPUTE_BIT ) )
            {
                break;
            }
        }
        if( idx == sz )
        {
            for( idx=0; idx<sz; idx++ )
            {
                if( ( qfp[idx].queueFlags & VK_QUEUE_COMPUTE_BIT ) )
                {
                    break;
                }
            }
        }
        m_queueInfo[(int)QueueType::Compute].idx = idx;
    }

    if( flags & RequireTransfer )
    {
        uint32_t idx = sz;
        for( idx=0; idx<sz; idx++ )
        {
            if( ( (int)idx != m_queueInfo[(int)QueueType::Graphic].idx ) &&
                ( (int)idx != m_queueInfo[(int)QueueType::Compute].idx ) &&
                ( qfp[idx].queueFlags & ( VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT ) ) )
            {
                break;
            }
        }
        if( idx == sz )
        {
            for( idx=0; idx<sz; idx++ )
            {
                if( qfp[idx].queueFlags & ( VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT ) )
                {
                    break;
                }
            }
        }
        m_queueInfo[(int)QueueType::Transfer].idx = idx;
    }

    if( m_queueInfo[(int)QueueType::Graphic].idx != -1 )
    {
        if( m_queueInfo[(int)QueueType::Graphic].idx == m_queueInfo[(int)QueueType::Compute].idx )
        {
            m_queueInfo[(int)QueueType::Graphic].shareCompute = true;
            m_queueInfo[(int)QueueType::Compute].shareGraphic = true;
        }
        if( m_queueInfo[(int)QueueType::Graphic].idx == m_queueInfo[(int)QueueType::Transfer].idx )
        {
            m_queueInfo[(int)QueueType::Graphic].shareTransfer = true;
            m_queueInfo[(int)QueueType::Transfer].shareGraphic = true;
        }
    }

    if( m_queueInfo[(int)QueueType::Compute].idx != -1 )
    {
        if( m_queueInfo[(int)QueueType::Compute].idx == m_queueInfo[(int)QueueType::Transfer].idx )
        {
            m_queueInfo[(int)QueueType::Compute].shareTransfer = true;
            m_queueInfo[(int)QueueType::Transfer].shareCompute = true;
        }
    }

    const float queuePriority = 1.0;
    std::vector<VkDeviceQueueCreateInfo> queueCreate;

    if( m_queueInfo[(int)QueueType::Graphic].idx >= 0 )
    {
        VkDeviceQueueCreateInfo qi = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
        qi.queueCount = 1;
        qi.queueFamilyIndex = (uint32_t)m_queueInfo[(int)QueueType::Graphic].idx;
        qi.pQueuePriorities = &queuePriority;
        queueCreate.emplace_back( qi );
    }

    if(  ( m_queueInfo[(int)QueueType::Compute].idx >= 0 ) &&
        !( m_queueInfo[(int)QueueType::Compute].shareGraphic ) )
    {
        VkDeviceQueueCreateInfo qi = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
        qi.queueCount = 1;
        qi.queueFamilyIndex = (uint32_t)m_queueInfo[(int)QueueType::Compute].idx;
        qi.pQueuePriorities = &queuePriority;
        queueCreate.emplace_back( qi );
    }

    if(  ( m_queueInfo[(int)QueueType::Transfer].idx >= 0 ) &&
        !( m_queueInfo[(int)QueueType::Transfer].shareGraphic ) &&
        !( m_queueInfo[(int)QueueType::Transfer].shareCompute ) )
    {
        VkDeviceQueueCreateInfo qi = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
        qi.queueCount = 1;
        qi.queueFamilyIndex = (uint32_t)m_queueInfo[(int)QueueType::Transfer].idx;
        qi.pQueuePriorities = &queuePriority;
        queueCreate.emplace_back( qi );
    }

    std::vector<const char*> deviceExtensions;

    VkDeviceCreateInfo devInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    devInfo.queueCreateInfoCount = (uint32_t)queueCreate.size();
    devInfo.pQueueCreateInfos = queueCreate.data();
    devInfo.enabledExtensionCount = (uint32_t)deviceExtensions.size();
    devInfo.ppEnabledExtensionNames = deviceExtensions.data();

    VkVerify( vkCreateDevice( physDev, &devInfo, nullptr, &m_device ) );

    if( m_queueInfo[(int)QueueType::Graphic].idx >= 0 )
    {
        vkGetDeviceQueue( m_device, m_queueInfo[(int)QueueType::Graphic].idx, 0, m_queue + (int)QueueType::Graphic );
    }
    if( m_queueInfo[(int)QueueType::Compute].idx >= 0 )
    {
        vkGetDeviceQueue( m_device, m_queueInfo[(int)QueueType::Compute].idx, 0, m_queue + (int)QueueType::Compute );
    }
    if( m_queueInfo[(int)QueueType::Transfer].idx >= 0 )
    {
        vkGetDeviceQueue( m_device, m_queueInfo[(int)QueueType::Transfer].idx, 0, m_queue + (int)QueueType::Transfer );
    }

    VmaAllocatorCreateInfo allocInfo = {};
    allocInfo.physicalDevice = physDev;
    allocInfo.device = m_device;
    allocInfo.instance = instance;

    VkVerify( vmaCreateAllocator( &allocInfo, &m_allocator ) );
}

VlkDevice::~VlkDevice()
{
    vmaDestroyAllocator( m_allocator );
    vkDestroyDevice( m_device, nullptr );
}

void VlkDevice::Submit( QueueType type, VkCommandBuffer cmdbuf, VkFence fence )
{
    VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdbuf;

    VkVerify( vkQueueSubmit( GetQueue( type ), 1, &submitInfo, fence ) );
}
