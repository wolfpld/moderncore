#include <array>
#include <format>
#include <string.h>
#include <tracy/Tracy.hpp>
#include <vector>

#include "VlkCommandBuffer.hpp"
#include "VlkCommandPool.hpp"
#include "VlkDevice.hpp"
#include "VlkError.hpp"
#include "VlkGarbage.hpp"
#include "VlkInstance.hpp"
#include "VlkPhysicalDevice.hpp"
#include "vulkan/ext/Tracy.hpp"

VlkDevice::VlkDevice( VlkInstance& instance, std::shared_ptr<VlkPhysicalDevice> physDev, int flags, VkSurfaceKHR presentSurface )
    : m_physDev( std::move( physDev ) )
    , m_garbage( std::make_shared<VlkGarbage>() )
    , m_queueInfo {}
    , m_queue {}
{
    ZoneScoped;
    ZoneVkDevice( m_physDev );

    CheckPanic( flags & ( RequireGraphic | RequireCompute ), "Requested Device without graphic and compute queues." );

    auto& qfp = m_physDev->QueueFamilyProperties();
    const auto sz = uint32_t( qfp.size() );

    bool presentOnGraphicsQueue = false;
    std::vector<VkBool32> presentSupport( sz );
    if( flags & RequirePresent )
    {
        if( presentSurface )
        {
            for( size_t i=0; i<sz; i++ )
            {
                vkGetPhysicalDeviceSurfaceSupportKHR( *m_physDev, uint32_t( i ), presentSurface, &presentSupport[i] );
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
        CheckPanic( m_physDev->IsGraphicCapable(), "Requested Device with graphic queue, but it does not has any graphic queues." );
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
        CheckPanic( m_physDev->IsComputeCapable(), "Requested Device with compute queue, but it does not has any compute queues." );
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

    // Always want transfer queue
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

    std::vector<const char*> deviceExtensions = { VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME };

    if( instance.Type() == VlkInstanceType::Wayland )
    {
        deviceExtensions.emplace_back( VK_KHR_SWAPCHAIN_EXTENSION_NAME );
        deviceExtensions.emplace_back( VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME );
    }
    else
    {
        deviceExtensions.emplace_back( VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME );
        deviceExtensions.emplace_back( VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME );
        deviceExtensions.emplace_back( VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME );
        deviceExtensions.emplace_back( VK_EXT_QUEUE_FAMILY_FOREIGN_EXTENSION_NAME );
        deviceExtensions.emplace_back( VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME );
    }

    VkPhysicalDeviceVulkan13Features features13 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .synchronization2 = VK_TRUE,
        .dynamicRendering = VK_TRUE
    };
    VkPhysicalDeviceVulkan12Features features12 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = &features13
    };
    VkPhysicalDeviceVulkan11Features features11 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
        .pNext = &features12
    };
    VkPhysicalDeviceFeatures2 features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &features11
    };

#ifdef TRACY_ENABLE
    CheckPanic( m_physDev->HasCalibratedTimestamps(), "Calibrated timestamps not supported, cannot profile." );
    deviceExtensions.emplace_back( VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME );
    features12.hostQueryReset = VK_TRUE;
#endif

    VkDeviceCreateInfo devInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    devInfo.pNext = &features;
    devInfo.queueCreateInfoCount = (uint32_t)queueCreate.size();
    devInfo.pQueueCreateInfos = queueCreate.data();
    devInfo.enabledExtensionCount = (uint32_t)deviceExtensions.size();
    devInfo.ppEnabledExtensionNames = deviceExtensions.data();

    auto res = vkCreateDevice( *m_physDev, &devInfo, nullptr, &m_device );
    if( res != VK_SUCCESS ) throw DeviceException( std::format( "Failed to create logical device ({}).", string_VkResult( res ) ) );

    if( instance.Type() == VlkInstanceType::Drm )
    {
        VkPhysicalDeviceExternalSemaphoreInfo extSemInfo = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO };
        extSemInfo.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT;

        VkExternalSemaphoreProperties extSemProps = { VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES };

        vkGetPhysicalDeviceExternalSemaphoreProperties( *m_physDev, &extSemInfo, &extSemProps );

        if( !( extSemProps.externalSemaphoreFeatures & VK_EXTERNAL_SEMAPHORE_FEATURE_IMPORTABLE_BIT ) )
        {
            throw DeviceException( "External semaphore import not supported." );
        }
    }

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
    allocInfo.physicalDevice = *m_physDev;
    allocInfo.device = m_device;
    allocInfo.instance = instance;
    allocInfo.vulkanApiVersion = instance.ApiVersion();

    VkVerify( vmaCreateAllocator( &allocInfo, &m_allocator ) );

    if( m_queueInfo[(int)QueueType::Graphic].idx >= 0 )
    {
        m_commandPool[(int)QueueType::Graphic] = std::make_shared<VlkCommandPool>( *this, m_queueInfo[(int)QueueType::Graphic].idx, QueueType::Graphic );
        m_queueLock[(int)QueueType::Graphic] = std::make_shared<std::mutex>();

        if( m_queueInfo[(int)QueueType::Graphic].shareCompute )
        {
            m_commandPool[(int)QueueType::Compute] = m_commandPool[(int)QueueType::Graphic];
            m_queueLock[(int)QueueType::Compute] = m_queueLock[(int)QueueType::Graphic];
        }

        if( m_queueInfo[(int)QueueType::Graphic].shareTransfer )
        {
            m_commandPool[(int)QueueType::Transfer] = m_commandPool[(int)QueueType::Graphic];
            m_queueLock[(int)QueueType::Transfer] = m_queueLock[(int)QueueType::Graphic];
        }
    }
    if( m_queueInfo[(int)QueueType::Compute].idx >= 0 )
    {
        if( !m_commandPool[(int)QueueType::Compute] )
        {
            m_commandPool[(int)QueueType::Compute] = std::make_shared<VlkCommandPool>( *this, m_queueInfo[(int)QueueType::Compute].idx, QueueType::Compute );
            m_queueLock[(int)QueueType::Compute] = std::make_shared<std::mutex>();
        }
        if( !m_commandPool[(int)QueueType::Transfer] && m_queueInfo[(int)QueueType::Compute].shareTransfer )
        {
            m_commandPool[(int)QueueType::Transfer] = m_commandPool[(int)QueueType::Compute];
            m_queueLock[(int)QueueType::Transfer] = m_queueLock[(int)QueueType::Compute];
        }
    }
    if( m_queueInfo[(int)QueueType::Transfer].idx >= 0 )
    {
        if( !m_commandPool[(int)QueueType::Transfer] )
        {
            m_commandPool[(int)QueueType::Transfer] = std::make_shared<VlkCommandPool>( *this, m_queueInfo[(int)QueueType::Transfer].idx, QueueType::Transfer );
            m_queueLock[(int)QueueType::Transfer] = std::make_shared<std::mutex>();
        }
    }

#ifdef TRACY_ENABLE
    auto gpdctd = (PFN_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT)vkGetInstanceProcAddr( instance, "vkGetPhysicalDeviceCalibrateableTimeDomainsEXT" );
    auto gct = (PFN_vkGetCalibratedTimestampsEXT)vkGetInstanceProcAddr( instance, "vkGetCalibratedTimestampsEXT" );

    uint32_t count;
    gpdctd( *m_physDev, &count, nullptr );
    std::vector<VkTimeDomainEXT> domains( count );
    gpdctd( *m_physDev, &count, domains.data() );

    CheckPanic( std::find( domains.begin(), domains.end(), VK_TIME_DOMAIN_CLOCK_MONOTONIC_EXT ) != domains.end(), "VK_TIME_DOMAIN_CLOCK_MONOTONIC_RAW_EXT not supported, cannot profile." );

    m_tracyCtx = TracyVkContextHostCalibrated( *m_physDev, m_device, vkResetQueryPool, gpdctd, gct );
    auto& properties = m_physDev->Properties();
    TracyVkContextName( m_tracyCtx, properties.deviceName, strlen( properties.deviceName ) );
    mclog( LogLevel::Info, "Vulkan profiling available on device %s", properties.deviceName );
#endif
}

VlkDevice::~VlkDevice()
{
#ifdef TRACY_ENABLE
    TracyVkDestroy( m_tracyCtx );
#endif

    m_garbage.reset();

    for( auto& pool : m_commandPool ) pool.reset();
    vmaDestroyAllocator( m_allocator );
    vkDestroyDevice( m_device, nullptr );
}

void VlkDevice::Submit( const VlkCommandBuffer& cmdbuf, VkFence fence )
{
    VkCommandBufferSubmitInfo cmdbufInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = cmdbuf
    };

    VkSubmitInfo2 submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &cmdbufInfo
    };

    VkVerify( vkQueueSubmit2( GetQueue( cmdbuf ), 1, &submitInfo, fence ) );
}
