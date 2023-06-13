#ifndef __VLKSHADERMODULE_HPP__
#define __VLKSHADERMODULE_HPP__

#include <vulkan/vulkan.h>

class VlkShaderModule
{
public:
    VlkShaderModule( VkDevice device, const char* data, size_t size );
    ~VlkShaderModule();

    operator VkShaderModule() const { return m_module; }

private:
    VkShaderModule m_module;
    VkDevice m_device;
};

#endif
