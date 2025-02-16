#include <array>
#include <string.h>

#include "Background.hpp"
#include "BackgroundFrag.hpp"
#include "BackgroundVert.hpp"
#include "util/EmbedData.hpp"
#include "vulkan/VlkBuffer.hpp"
#include "vulkan/VlkDevice.hpp"
#include "vulkan/VlkShader.hpp"
#include "vulkan/VlkShaderModule.hpp"
#include "vulkan/ext/GarbageChute.hpp"

struct Vertex
{
    float x, y;
};

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

    constexpr Vertex vdata[] = {
        { -1, -1 },
        { -1,  3 },
        {  3, -1 }
    };
    constexpr VkBufferCreateInfo vinfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof( vdata ),
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    m_vertexBuffer = std::make_shared<VlkBuffer>( *m_device, vinfo, VlkBuffer::PreferDevice | VlkBuffer::WillWrite );
    memcpy( m_vertexBuffer->Ptr(), vdata, sizeof( vdata ) );
    m_vertexBuffer->Flush();
}

Background::~Background()
{
    m_garbage.Recycle( std::move( m_shader ) );
    m_garbage.Recycle( std::move( m_vertexBuffer ) );
}
