#include <array>
#include <assert.h>
#include <string.h>
#include <tracy/Tracy.hpp>
#include <vector>

#include "VlkCommandBuffer.hpp"
#include "VlkCommandPool.hpp"
#include "VlkDevice.hpp"
#include "VlkError.hpp"
#include "VlkInstance.hpp"
#include "VlkPhysicalDevice.hpp"
#include "util/Panic.hpp"
#include "util/Tracy.hpp"

VlkDevice::VlkDevice( VlkInstance& instance, VkPhysicalDevice physDev, int flags, VkSurfaceKHR presentSurface )
    : m_physDev( physDev )
    , m_queueInfo {}
    , m_queue {}
{
    ZoneScoped;
    ZoneVkDevice( physDev );

    CheckPanic( flags & ( RequireGraphic | RequireCompute ), "Requested Device without graphic and compute queues." );

    VlkPhysicalDevice phys( physDev );
    auto& qfp = phys.GetQueueFamilyProperties();
    const auto sz = uint32_t( qfp.size() );

    bool presentOnGraphicsQueue = false;
    std::vector<VkBool32> presentSupport( sz );
    if( flags & RequirePresent )
    {
        if( presentSurface )
        {
            for( size_t i=0; i<sz; i++ )
            {
                vkGetPhysicalDeviceSurfaceSupportKHR( physDev, uint32_t( i ), presentSurface, &presentSupport[i] );
            }
        }
        else
        {
            mclog( LogLevel::Debug, "No present surface provided, assuming all queues support present." );
            std::fill( presentSupport.begin(), presentSupport.end(), VK_TRUE );
        }
    }

    if( flags & RequireGraphic )
    {
        CheckPanic( phys.IsGraphicCapable(), "Requested Device with graphic queue, but it does not has any graphic queues." );
        uint32_t idx = sz;
        if( flags & RequirePresent )
        {
            if( flags & RequireCompute )
            {
                for( idx=0; idx<sz; idx++ )
                {
                    if(  ( qfp[idx].queueFlags & VK_QUEUE_GRAPHICS_BIT ) &&
                        !( qfp[idx].queueFlags & VK_QUEUE_COMPUTE_BIT  ) &&
                        presentSupport[idx] )
                    {
                        presentOnGraphicsQueue = true;
                        break;
                    }
                }
            }
            if( idx == sz )
            {
                for( idx=0; idx<sz; idx++ )
                {
                    if( ( qfp[idx].queueFlags & VK_QUEUE_GRAPHICS_BIT ) &&
                        presentSupport[idx] )
                    {
                        presentOnGraphicsQueue = true;
                        break;
                    }
                }
            }
        }
        if( idx == sz && ( flags & RequireCompute ) )
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
        CheckPanic( idx != sz, "Requested Device with graphic queue, but it does not has any graphic queues." );
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

    if( flags & RequirePresent )
    {
        if( presentOnGraphicsQueue )
        {
            m_queueInfo[(int)QueueType::Present].idx = m_queueInfo[(int)QueueType::Graphic].idx;
        }
        else
        {
            uint32_t idx;
            for( idx=0; idx<sz; idx++ )
            {
                if( presentSupport[idx] )
                {
                    if( m_queueInfo[(int)QueueType::Compute].idx == idx ) continue;
                    if( m_queueInfo[(int)QueueType::Transfer].idx == idx ) continue;

                    m_queueInfo[(int)QueueType::Present].idx = idx;
                    break;
                }
            }
            if( idx == sz )
            {
                for( idx=0; idx<sz; idx++ )
                {
                    if( presentSupport[idx] )
                    {
                        m_queueInfo[(int)QueueType::Present].idx = idx;
                        break;
                    }
                }
            }
        }
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
        if( m_queueInfo[(int)QueueType::Graphic].idx == m_queueInfo[(int)QueueType::Present].idx )
        {
            m_queueInfo[(int)QueueType::Graphic].sharePresent = true;
            m_queueInfo[(int)QueueType::Present].shareGraphic = true;
        }
    }

    if( m_queueInfo[(int)QueueType::Compute].idx != -1 )
    {
        if( m_queueInfo[(int)QueueType::Compute].idx == m_queueInfo[(int)QueueType::Transfer].idx )
        {
            m_queueInfo[(int)QueueType::Compute].shareTransfer = true;
            m_queueInfo[(int)QueueType::Transfer].shareCompute = true;
        }
        if( m_queueInfo[(int)QueueType::Compute].idx == m_queueInfo[(int)QueueType::Present].idx )
        {
            m_queueInfo[(int)QueueType::Compute].sharePresent = true;
            m_queueInfo[(int)QueueType::Present].shareCompute = true;
        }
    }

    if( m_queueInfo[(int)QueueType::Transfer].idx != -1 )
    {
        if( m_queueInfo[(int)QueueType::Transfer].idx == m_queueInfo[(int)QueueType::Present].idx )
        {
            m_queueInfo[(int)QueueType::Transfer].sharePresent = true;
            m_queueInfo[(int)QueueType::Present].shareTransfer = true;
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

    if(  ( m_queueInfo[(int)QueueType::Present].idx >= 0 ) &&
        !( m_queueInfo[(int)QueueType::Present].shareGraphic ) &&
        !( m_queueInfo[(int)QueueType::Present].shareCompute ) &&
        !( m_queueInfo[(int)QueueType::Present].shareTransfer ) )
    {
        VkDeviceQueueCreateInfo qi = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
        qi.queueCount = 1;
        qi.queueFamilyIndex = (uint32_t)m_queueInfo[(int)QueueType::Present].idx;
        qi.pQueuePriorities = &queuePriority;
        queueCreate.emplace_back( qi );
    }

    constexpr std::array deviceExtensions {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME
    };

    VkDeviceCreateInfo devInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    devInfo.queueCreateInfoCount = (uint32_t)queueCreate.size();
    devInfo.pQueueCreateInfos = queueCreate.data();
    devInfo.enabledExtensionCount = (uint32_t)deviceExtensions.size();
    devInfo.ppEnabledExtensionNames = deviceExtensions.data();

    VkVerify( vkCreateDevice( physDev, &devInfo, nullptr, &m_device ) );

    if( m_queueInfo[(int)QueueType::Graphic].idx >= 0 )
    {
        vkGetDeviceQueue( m_device, m_queueInfo[(int)QueueType::Graphic].idx, 0, &m_queue[(int)QueueType::Graphic] );
    }
    if( m_queueInfo[(int)QueueType::Compute].idx >= 0 )
    {
        vkGetDeviceQueue( m_device, m_queueInfo[(int)QueueType::Compute].idx, 0, &m_queue[(int)QueueType::Compute] );
    }
    if( m_queueInfo[(int)QueueType::Transfer].idx >= 0 )
    {
        vkGetDeviceQueue( m_device, m_queueInfo[(int)QueueType::Transfer].idx, 0, &m_queue[(int)QueueType::Transfer] );
    }
    if( m_queueInfo[(int)QueueType::Present].idx >= 0 )
    {
        vkGetDeviceQueue( m_device, m_queueInfo[(int)QueueType::Present].idx, 0, &m_queue[(int)QueueType::Present] );
    }

    VmaAllocatorCreateInfo allocInfo = {};
    allocInfo.physicalDevice = physDev;
    allocInfo.device = m_device;
    allocInfo.instance = instance;
    allocInfo.vulkanApiVersion = VK_API_VERSION_1_1;

    VkVerify( vmaCreateAllocator( &allocInfo, &m_allocator ) );

    if( m_queueInfo[(int)QueueType::Graphic].idx >= 0 )
    {
        m_commandPool[(int)QueueType::Graphic] = std::make_shared<VlkCommandPool>( *this, m_queueInfo[(int)QueueType::Graphic].idx, QueueType::Graphic );
        if( m_queueInfo[(int)QueueType::Graphic].shareCompute ) m_commandPool[(int)QueueType::Compute] = m_commandPool[(int)QueueType::Graphic];
        if( m_queueInfo[(int)QueueType::Graphic].shareTransfer ) m_commandPool[(int)QueueType::Transfer] = m_commandPool[(int)QueueType::Graphic];
    }
    if( m_queueInfo[(int)QueueType::Compute].idx >= 0 )
    {
        assert( !m_commandPool[(int)QueueType::Compute] );
        m_commandPool[(int)QueueType::Compute] = std::make_shared<VlkCommandPool>( *this, m_queueInfo[(int)QueueType::Compute].idx, QueueType::Compute );
        if( m_queueInfo[(int)QueueType::Compute].shareTransfer ) m_commandPool[(int)QueueType::Transfer] = m_commandPool[(int)QueueType::Compute];
    }
    if( m_queueInfo[(int)QueueType::Transfer].idx >= 0 )
    {
        assert( !m_commandPool[(int)QueueType::Transfer] );
        if( !m_commandPool[(int)QueueType::Transfer] ) m_commandPool[(int)QueueType::Transfer] = std::make_shared<VlkCommandPool>( *this, m_queueInfo[(int)QueueType::Transfer].idx, QueueType::Transfer );
    }
}

VlkDevice::~VlkDevice()
{
    for( auto& pool : m_commandPool ) pool.reset();
    vmaDestroyAllocator( m_allocator );
    vkDestroyDevice( m_device, nullptr );
}

void VlkDevice::Submit( const VlkCommandBuffer& cmdbuf, VkFence fence )
{
    const std::array<VkCommandBuffer, 1> cmdbufs = { cmdbuf };

    VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submitInfo.commandBufferCount = (uint32_t)cmdbufs.size();
    submitInfo.pCommandBuffers = cmdbufs.data();

    VkVerify( vkQueueSubmit( GetQueue( cmdbuf ), 1, &submitInfo, fence ) );
}
