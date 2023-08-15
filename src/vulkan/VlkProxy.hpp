#pragma once

#include <vulkan/vulkan.h>

VkResult LoadVulkanExtensions( VkInstance instance );

VkResult CreateDebugUtilsMessengerEXT( VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger );
void DestroyDebugUtilsMessengerEXT( VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator );

extern PFN_vkCmdPushDescriptorSetKHR CmdPushDescriptorSetKHR;
