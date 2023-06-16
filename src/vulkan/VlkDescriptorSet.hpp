#ifndef __VLKDESCRIPTORSET_HPP__
#define __VLKDESCRIPTORSET_HPP__

#include <vulkan/vulkan.h>

class VlkDescriptorSetLayout;

class VlkDescriptorSet
{
public:
    explicit VlkDescriptorSet( const VlkDescriptorSetLayout& layout );
    ~VlkDescriptorSet();

    operator VkDescriptorSet() const { return m_descriptorSet; }

private:
    VkDescriptorSet m_descriptorSet;
    const VlkDescriptorSetLayout& m_layout;
};

#endif
