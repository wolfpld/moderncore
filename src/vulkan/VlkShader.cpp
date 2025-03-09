#include "VlkShader.hpp"
#include "VlkShaderModule.hpp"

VlkShader::VlkShader( const Stage* stages, size_t count )
    : m_stages( count )
    , m_modules( count )
{
    for( size_t i=0; i<count; i++ )
    {
        m_stages[i] = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = stages[i].stage,
            .module = *stages[i].module,
            .pName = "main"
        };
        m_modules[i] = stages[i].module;
    }
}
