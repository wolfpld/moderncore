#pragma once

#include <vulkan/vulkan.h>

#include "VlkBase.hpp"
#include "util/DataContainer.hpp"
#include "util/NoCopy.hpp"

class VlkShaderModule : public VlkBase
{
public:
    template<DataContainer T>
    VlkShaderModule( VkDevice device, T& data ) : VlkShaderModule( device, data.data(), data.size() ) {}

    VlkShaderModule( VkDevice device, const char* data, size_t size );
    ~VlkShaderModule();

    NoCopy( VlkShaderModule );

    operator VkShaderModule() const { return m_module; }

private:
    VkShaderModule m_module;
    VkDevice m_device;
};
