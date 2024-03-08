#include <algorithm>
#include <array>
#include <string.h>
#include <tracy/Tracy.hpp>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_wayland.h>

#include "VlkError.hpp"
#include "VlkInstance.hpp"
#include "VlkProxy.hpp"
#include "util/Logs.hpp"
#include "util/Panic.hpp"
#include "util/TaskDispatch.hpp"
#include "vulkan/VlkPhysicalDevice.hpp"

constexpr std::array validationLayers = { "VK_LAYER_KHRONOS_validation" };

static bool HasValidationLayers()
{
    uint32_t numLayers;
    vkEnumerateInstanceLayerProperties( &numLayers, nullptr );
    std::vector<VkLayerProperties> layers( numLayers );
    vkEnumerateInstanceLayerProperties( &numLayers, layers.data() );

    for( auto& search : validationLayers )
    {
        if( std::none_of( layers.begin(), layers.end(), [&search](auto& layer){ return strcmp( layer.layerName, search ) == 0; } ) )
        {
            mclog( LogLevel::Warning, "Validation layers are not available!" );
            return false;
        }
    }

    return true;
}

[[maybe_unused]] static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback( VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                                                      VkDebugUtilsMessageTypeFlagsEXT type,
                                                                      const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                                                      void* pUserData )
{
    LogLevel loglevel;
    switch( severity )
    {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        loglevel = LogLevel::Debug;
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        loglevel = LogLevel::Warning;
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        loglevel = LogLevel::Error;
        break;
    default:
        return VK_FALSE;
    }

    const char* t;
    switch( type )
    {
    case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:
        t = "";
        break;
    case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:
        t = "[Validation]";
        break;
    case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT:
        t = "[Perf]";
        break;
    default:
        CheckPanic( false, "Unknown debug message type" );
        break;
    }

    mclog( loglevel, "%s %s", t, pCallbackData->pMessage );
    return VK_FALSE;
}

VlkInstance::VlkInstance( VlkInstanceType instanceType, bool enableValidation )
    : m_debugMessenger( VK_NULL_HANDLE )
    , m_instanceType( instanceType )
{
    ZoneScoped;

    VkApplicationInfo appInfo = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
    appInfo.pApplicationName = "Modern Core";
    appInfo.applicationVersion = VK_MAKE_API_VERSION( 0, 1, 0, 0 );
    appInfo.pEngineName = "Modern Core";
    appInfo.engineVersion = VK_MAKE_API_VERSION( 0, 0, 0, 0 );
    appInfo.apiVersion = ApiVersion();

    VkInstanceCreateInfo createInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    createInfo.pApplicationInfo = &appInfo;

    std::vector<const char*> instanceExtensions;

    switch( instanceType )
    {
    case VlkInstanceType::Wayland:
        instanceExtensions.emplace_back( VK_KHR_SURFACE_EXTENSION_NAME );
        instanceExtensions.emplace_back( VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME );
        break;
    default:
        break;
    }

    VkDebugUtilsMessengerCreateInfoEXT dbgInfo = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };

    const bool hasValidationLayers = enableValidation && HasValidationLayers();
    if( hasValidationLayers )
    {
        dbgInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        dbgInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        dbgInfo.pfnUserCallback = DebugCallback;

        createInfo.enabledLayerCount = (uint32_t)validationLayers.size();
        createInfo.ppEnabledLayerNames = validationLayers.data();
        createInfo.pNext = &dbgInfo;

        instanceExtensions.emplace_back( VK_EXT_DEBUG_UTILS_EXTENSION_NAME );
    }

    createInfo.enabledExtensionCount = (uint32_t)instanceExtensions.size();
    createInfo.ppEnabledExtensionNames = instanceExtensions.data();

    VkVerify( vkCreateInstance( &createInfo, nullptr, &m_instance ) );
    if( hasValidationLayers ) VkVerify( CreateDebugUtilsMessengerEXT( m_instance, &dbgInfo, nullptr, &m_debugMessenger ) );

    VkVerify( LoadVulkanExtensions( *this ) );
}

VlkInstance::~VlkInstance()
{
    DestroyDebugUtilsMessengerEXT( m_instance, m_debugMessenger, nullptr );
    vkDestroyInstance( m_instance, nullptr );
}

void VlkInstance::InitPhysicalDevices( TaskDispatch& dispatch )
{
    ZoneScoped;

    std::vector<VkPhysicalDevice> phys;
    uint32_t cnt;
    vkEnumeratePhysicalDevices( m_instance, &cnt, nullptr );
    phys.resize( cnt );
    vkEnumeratePhysicalDevices( m_instance, &cnt, phys.data() );

    m_physicalDevices.resize( phys.size() );
    size_t i = 0;
    for( auto& p : phys )
    {
        dispatch.Queue( [this, p, i] {
            m_physicalDevices[i] = std::make_shared<VlkPhysicalDevice>( p );
        } );
        i++;
    }

    dispatch.Sync();

    auto it = m_physicalDevices.begin();
    while( it != m_physicalDevices.end() )
    {
        if( !(*it)->HasDynamicRendering() || !(*it)->HasPushDescriptor() || !(*it)->IsGraphicCapable() )
        {
            mclog( LogLevel::Warning, "Physical device '%s' is not compatible", (*it)->Properties().deviceName );
            it = m_physicalDevices.erase( it );
        }
        else
        {
            ++it;
        }
    }
}
