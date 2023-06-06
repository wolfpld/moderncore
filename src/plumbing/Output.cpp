#include <assert.h>

#include "Allocator.hpp"
#include "Backend.hpp"
#include "Output.hpp"
#include "OutputNode.hpp"
#include "Renderer.hpp"
#include "Scene.hpp"
#include "../util/Logs.hpp"
#include "../util/Panic.hpp"

extern "C" {
#define WLR_USE_UNSTABLE
#include <wlr/backend.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>

/* wlr_scene.h header uses C99 features not recognized by a C++ compiler */
bool wlr_scene_attach_output_layout(struct wlr_scene *scene, struct wlr_output_layout *output_layout);
};

Output::Output( const BackendWlr& backend, const Allocator& allocator, const Renderer& renderer, Scene& scene )
    : m_allocator( allocator )
    , m_renderer( renderer )
    , m_scene( scene )
    , m_layout( wlr_output_layout_create() )
    , m_newOutput( backend.Get()->events.new_output, [this](auto v){ NewOutput( v ); } )
{
    CheckPanic( m_layout, "Failed to create wlr_output_layout!" );

    wlr_scene_attach_output_layout( m_scene, m_layout );
}

Output::~Output()
{
    wlr_output_layout_destroy( m_layout );
}

void Output::NewOutput( wlr_output* output )
{
    mclog( LogLevel::Info, "New output: %s (%s %s), %ix%i mm, %ix%i px, %.2f Hz",
        output->name,
        output->make ? output->make : "null",
        output->model ? output->model : "null",
        output->phys_width,
        output->phys_height,
        output->width,
        output->height,
        output->refresh / 1000.f );

    wlr_output_init_render( output, m_allocator, m_renderer );

    if( !wl_list_empty( &output->modes ) )
    {
        auto preferred = wlr_output_preferred_mode( output );
        wlr_output_set_mode( output, preferred );
        wlr_output_enable( output, true );
        if( !wlr_output_commit( output ) )
        {
            mclog( LogLevel::Error, "Cannot set preferred mode for output: %ix%i %.2f Hz", preferred->width, preferred->height, preferred->refresh / 1000.f );
            return;
        }
    }

    m_nodes.emplace_back( std::make_unique<OutputNode>( output, m_scene, *this ) );

    wlr_output_layout_add_auto( m_layout, output );
}

void Output::Remove( const OutputNode* node )
{
    auto it = std::find_if( m_nodes.begin(), m_nodes.end(), [node]( const auto& v ) { return v.get() == node; } );
    assert( it != m_nodes.end() );
    m_nodes.erase( it );
}
