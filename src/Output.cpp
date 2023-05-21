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

static Output* s_instance;

Output::Output( const Backend& backend )
    : m_layout( wlr_output_layout_create() )
{
    CheckPanic( m_layout, "Failed to create wlr_output_layout!" );

    CheckPanic( !s_instance, "Creating a second instance of Output!" );
    s_instance = this;

    wl_list_init( &m_outputs );
    m_newOutput.notify = []( struct wl_listener* listener, void* data ){ s_instance->NewOutput( (struct wlr_output*)data ); };
    wl_signal_add( &backend.Get()->events.new_output, &m_newOutput );
}

Output::~Output()
{
    wl_list_remove( &m_newOutput.link );
    wlr_output_layout_destroy( m_layout );
    s_instance = nullptr;
}

void Output::NewOutput( struct wlr_output* output )
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
