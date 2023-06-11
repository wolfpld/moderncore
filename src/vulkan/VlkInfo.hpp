#ifndef __VLKINFO_HPP__
#define __VLKINFO_HPP__

#include <vulkan/vulkan.h>

class VlkDevice;
class VlkSwapchainProperties;

void PrintVulkanPhysicalDevice( VkPhysicalDevice physDevice );
void PrintVulkanQueues( const VlkDevice& device );
void PrintSwapchainProperties( const VlkSwapchainProperties& properties );

#endif
