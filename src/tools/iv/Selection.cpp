#include "Selection.hpp"
#include "util/EmbedData.hpp"
#include "vulkan/VlkDevice.hpp"
#include "vulkan/VlkShader.hpp"
#include "vulkan/VlkShaderModule.hpp"
#include "vulkan/ext/GarbageChute.hpp"

#include "shader/SelectionFrag.hpp"
#include "shader/SelectionVert.hpp"

Selection::Selection( GarbageChute& garbage, std::shared_ptr<VlkDevice> device, VkFormat format )
    : m_garbage( garbage )
    , m_device( std::move( device ) )
{
    Unembed( SelectionVert );
    Unembed( SelectionFrag );

    const std::array stages = {
        VlkShader::Stage { std::make_shared<VlkShaderModule>( *m_device, *SelectionVert ), VK_SHADER_STAGE_VERTEX_BIT },
        VlkShader::Stage { std::make_shared<VlkShaderModule>( *m_device, *SelectionFrag ), VK_SHADER_STAGE_FRAGMENT_BIT }
    };
    m_shader = std::make_shared<VlkShader>( stages );


    const std::array stagesPq = {
        stages[0],
        VlkShader::Stage { std::make_shared<VlkShaderModule>( *m_device, *SelectionFrag ), VK_SHADER_STAGE_FRAGMENT_BIT }
    };
    m_shaderPq = std::make_shared<VlkShader>( stagesPq );
}

Selection::~Selection()
{
    m_garbage.Recycle( {
        std::move( m_shader ),
        std::move( m_shaderPq )
    } );
}
