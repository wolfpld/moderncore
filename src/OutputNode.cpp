#include "OutputNode.hpp"
#include "Scene.hpp"

extern "C" {
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_output.h>
#include <wlr/util/log.h>
};

OutputNode::OutputNode( wlr_output* output, Scene& scene )
    : m_scene( scene )
    , m_output( output )
    , m_frame( m_output->events.frame, [this](auto v){ Frame(); } )
    , m_destroy( m_output->events.destroy, [this](auto v){ Destroy(); } )
{
}

void OutputNode::Frame()
{
    wlr_log( WLR_INFO, "New frame" );
    m_scene.Render( m_output );
}

void OutputNode::Destroy()
{
}
