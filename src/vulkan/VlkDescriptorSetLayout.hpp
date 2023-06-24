#ifndef __VLKDESCRIPTORSETLAYOUT_HPP__
#define __VLKDESCRIPTORSETLAYOUT_HPP__

#include <vulkan/vulkan.h>

#include "VlkDevice.hpp"
#include "../util/NoCopy.hpp"

class VlkDescriptorSetLayout
{
public:
    explicit VlkDescriptorSetLayout( const VlkDevice& device, const VkDescriptorSetLayoutCreateInfo& createInfo );
    ~VlkDescriptorSetLayout();

    NoCopy( VlkDescriptorSetLayout );

    operator VkDescriptorSetLayout() const { return m_descriptorSetLayout; }
    operator VkDevice() const { return m_device; }

private:
    const VlkDevice& m_device;
    VkDescriptorSetLayout m_descriptorSetLayout;
};

#endif
