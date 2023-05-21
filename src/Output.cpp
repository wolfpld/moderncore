#include "Backend.hpp"
#include "Panic.hpp"
#include "Output.hpp"

extern "C" {
#define WLR_USE_UNSTABLE
#include <wlr/backend.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/util/log.h>
};

Output::Output( const Backend& backend )
    : m_layout( wlr_output_layout_create() )
    , m_newOutput( backend.Get()->events.new_output, [this](auto v){ NewOutput( v ); } )
{
    CheckPanic( m_layout, "Failed to create wlr_output_layout!" );

    wl_list_init( &m_outputs );
}

Output::~Output()
{
    wlr_output_layout_destroy( m_layout );
}

void Output::NewOutput( wlr_output* output )
{
    wlr_log( WLR_INFO, "New output: %s (%s %s), %ix%i mm, %ix%i px, %.2f Hz",
        output->name,
        output->make ? output->make : "null",
        output->model ? output->model : "null",
        output->phys_width,
        output->phys_height,
        output->width,
        output->height,
        output->refresh / 1000.f );
}
