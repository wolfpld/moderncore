#pragma once

#include <memory>
#include <stdint.h>
#include <vector>
#include <vulkan/vulkan.h>

#include "util/NoCopy.hpp"

class VlkBase;
class VlkDescriptorSetLayout;
class VlkDevice;
class VlkPipeline;
class VlkPipelineLayout;
class VlkSampler;
class VlkShaderModule;

class MipGen
{
public:
    explicit MipGen( VlkDevice& device );
    NoCopy( MipGen );

    [[nodiscard]] static constexpr VkFormat StorageViewFormat( VkFormat format )
    {
        return format == VK_FORMAT_R8G8B8A8_SRGB ? VK_FORMAT_R8G8B8A8_UNORM : format;
    }

    [[nodiscard]] bool Supports( VkFormat format, uint32_t width, uint32_t height ) const;

    void GenerateChain( VkCommandBuffer cmdBuf, VkImage image, VkFormat format,
        uint32_t width, uint32_t height, uint32_t mipLevels,
        uint32_t sourceFamily, uint32_t releaseFamily,
        std::vector<std::shared_ptr<VlkBase>>& keepAlive );

private:
    VlkDevice& m_device;
    uint32_t m_computeFamily;

    std::unique_ptr<VlkShaderModule> m_module;
    std::unique_ptr<VlkSampler> m_sampler;
    std::unique_ptr<VlkDescriptorSetLayout> m_setLayout;
    std::unique_ptr<VlkPipelineLayout> m_pipelineLayout;
    std::unique_ptr<VlkPipeline> m_pipeline;
};
