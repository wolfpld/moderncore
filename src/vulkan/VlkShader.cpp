#include "VlkShader.hpp"
#include "VlkShaderModule.hpp"

VlkShader::VlkShader( Stage* stages, size_t count )
    : m_stages( count )
    , m_modules( count )
{
    for( size_t i=0; i<count; i++ )
    {
        m_stages[i] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        m_stages[i].stage = stages[i].stage;
        m_stages[i].module = *stages[i].module;
        m_stages[i].pName = "main";

        m_modules[i] = std::move( stages[i].module );
    }
}
