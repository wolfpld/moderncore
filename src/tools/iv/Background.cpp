#include <array>

#include "Background.hpp"
#include "BackgroundFrag.hpp"
#include "BackgroundVert.hpp"
#include "util/EmbedData.hpp"
#include "vulkan/VlkDevice.hpp"
#include "vulkan/VlkShader.hpp"
#include "vulkan/VlkShaderModule.hpp"
#include "vulkan/ext/GarbageChute.hpp"

Background::Background( GarbageChute& garbage, std::shared_ptr<VlkDevice> device )
    : m_garbage( garbage )
    , m_device( std::move( device ) )
{
    Unembed( BackgroundVert );
    Unembed( BackgroundFrag );

    std::array stages = {
        VlkShader::Stage { std::make_shared<VlkShaderModule>( *m_device, BackgroundVert ), VK_SHADER_STAGE_VERTEX_BIT },
        VlkShader::Stage { std::make_shared<VlkShaderModule>( *m_device, BackgroundFrag ), VK_SHADER_STAGE_FRAGMENT_BIT }
    };

    m_shader = std::make_shared<VlkShader>( stages );
}

Background::~Background()
{
    m_garbage.Recycle( std::move( m_shader ) );
}
