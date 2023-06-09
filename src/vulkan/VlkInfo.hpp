#ifndef __VLKINFO_HPP__
#define __VLKINFO_HPP__

#include <vulkan/vulkan.h>

class VlkDevice;

void PrintVulkanPhysicalDevice( VkPhysicalDevice physDevice );
void PrintVulkanQueues( const VlkDevice& device );

#endif
