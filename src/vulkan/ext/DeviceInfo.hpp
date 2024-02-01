#pragma once

#include <vulkan/vulkan.h>

class VlkDevice;

void PrintPhysicalDeviceInfo( VkPhysicalDevice physDev );
void PrintQueueConfig( const VlkDevice& device );
