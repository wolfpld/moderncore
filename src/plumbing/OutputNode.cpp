#include "Output.hpp"
#include "OutputNode.hpp"
#include "Scene.hpp"
#include "../util/Logs.hpp"

extern "C" {
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_output.h>
};

OutputNode::OutputNode( wlr_output* output, Scene& scene, Output& parent )
    : m_scene( scene )
    , m_parent( parent )
    , m_output( output )
    , m_frame( m_output->events.frame, [this](auto){ Frame(); } )
    , m_destroy( m_output->events.destroy, [this](auto){ Destroy(); } )
{
}

void OutputNode::Frame()
{
    mclog( LogLevel::Debug, "%s: New frame", m_output->name );
    m_scene.Render( m_output );
}

void OutputNode::Destroy()
{
    mclog( LogLevel::Info, "Remove output %s", m_output->name );
    m_parent.Remove( this );
}
