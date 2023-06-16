#include <array>

#include "VlkError.hpp"
#include "VlkDescriptorSet.hpp"
#include "VlkDescriptorSetLayout.hpp"

VlkDescriptorSet::VlkDescriptorSet( const VlkDescriptorSetLayout& layout )
    : m_layout( layout )
{
    const std::array<VkDescriptorSetLayout, 1> layouts = { layout };

    VkDescriptorSetAllocateInfo allocateInfo = {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocateInfo.descriptorPool = layout;
    allocateInfo.descriptorSetCount = (uint32_t)layouts.size();
    allocateInfo.pSetLayouts = layouts.data();

    VkVerify( vkAllocateDescriptorSets( layout, &allocateInfo, &m_descriptorSet ) );
}

VlkDescriptorSet::~VlkDescriptorSet()
{
    vkFreeDescriptorSets( m_layout, m_layout, 1, &m_descriptorSet );
}
