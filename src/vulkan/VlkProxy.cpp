#include "VlkProxy.hpp"

PFN_vkCmdPushDescriptorSetKHR CmdPushDescriptorSetKHR;

VkResult LoadVulkanExtensions( VkInstance instance )
{
    CmdPushDescriptorSetKHR = (PFN_vkCmdPushDescriptorSetKHR)vkGetInstanceProcAddr( instance, "vkCmdPushDescriptorSetKHR" );
    if( !CmdPushDescriptorSetKHR ) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return VK_SUCCESS;
}

VkResult CreateDebugUtilsMessengerEXT( VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger )
{
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr( instance, "vkCreateDebugUtilsMessengerEXT" );
    if( !func ) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return func( instance, pCreateInfo, pAllocator, pDebugMessenger );
}

void DestroyDebugUtilsMessengerEXT( VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator )
{
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr( instance, "vkDestroyDebugUtilsMessengerEXT" );
    if( func ) func( instance, debugMessenger, pAllocator );
}
