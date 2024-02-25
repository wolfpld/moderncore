#pragma once

#include <vulkan/vulkan.h>

class VlkInstance;

VkResult LoadVulkanExtensions( VlkInstance& instance );

VkResult CreateDebugUtilsMessengerEXT( VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger );
void DestroyDebugUtilsMessengerEXT( VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator );

extern PFN_vkCmdPushDescriptorSetKHR CmdPushDescriptorSetKHR;
extern PFN_vkGetSemaphoreFdKHR GetSemaphoreFdKHR;
extern PFN_vkImportSemaphoreFdKHR ImportSemaphoreFdKHR;
extern PFN_vkGetMemoryFdPropertiesKHR GetMemoryFdPropertiesKHR;
