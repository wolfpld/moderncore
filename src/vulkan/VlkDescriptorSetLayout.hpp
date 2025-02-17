#pragma once

#include <vulkan/vulkan.h>

#include "VlkBase.hpp"
#include "VlkDevice.hpp"
#include "util/NoCopy.hpp"

class VlkDescriptorSetLayout : public VlkBase
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
