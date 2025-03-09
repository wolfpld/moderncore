#include "VlkError.hpp"
#include "VlkShaderModule.hpp"

VlkShaderModule::VlkShaderModule( VkDevice device, const char* data, size_t size )
    : m_device( device )
{
    const VkShaderModuleCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = size,
        .pCode = (const uint32_t*)data
    };
    VkVerify( vkCreateShaderModule( device, &info, nullptr, &m_module ) );
}

VlkShaderModule::~VlkShaderModule()
{
    vkDestroyShaderModule( m_device, m_module, nullptr );
}
