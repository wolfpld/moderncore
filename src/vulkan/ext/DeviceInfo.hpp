#pragma once

#include <vulkan/vulkan.h>

class VlkDevice;
class VlkPhysicalDevice;

void PrintPhysicalDeviceInfo( const VlkPhysicalDevice& physDev );
void PrintQueueConfig( const VlkDevice& device );
