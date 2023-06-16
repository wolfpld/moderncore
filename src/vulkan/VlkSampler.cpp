#include "VlkError.hpp"
#include "VlkSampler.hpp"

VlkSampler::VlkSampler( VkDevice device, const VkSamplerCreateInfo& createInfo )
    : m_device( device )
{
    VkVerify( vkCreateSampler( m_device, &createInfo, nullptr, &m_sampler ) );
}

VlkSampler::~VlkSampler()
{
    vkDestroySampler( m_device, m_sampler, nullptr );
}
