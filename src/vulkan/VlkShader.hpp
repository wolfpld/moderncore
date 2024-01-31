#pragma once

#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

#include "util/DataContainer.hpp"
#include "util/NoCopy.hpp"

class VlkShaderModule;

class VlkShader
{
public:
    struct Stage
    {
        std::shared_ptr<VlkShaderModule> module;    // Shader ctor will move this into m_modules
        VkShaderStageFlagBits stage;
    };

    template<DataContainer T>
    explicit VlkShader( T& stages ) : VlkShader( stages.data(), stages.size() ) {}

    VlkShader( Stage* stages, size_t count );

    NoCopy( VlkShader );

    [[nodiscard]] uint32_t GetStageCount() const { return m_stages.size(); }
    [[nodiscard]] const VkPipelineShaderStageCreateInfo* GetStages() const { return m_stages.data(); }

private:
    std::vector<VkPipelineShaderStageCreateInfo> m_stages;
    std::vector<std::shared_ptr<VlkShaderModule>> m_modules;
};
