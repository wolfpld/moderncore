#include "Backend.hpp"
#include "Panic.hpp"
#include "Output.hpp"

extern "C" {
#define WLR_USE_UNSTABLE
#include <wlr/backend.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/util/log.h>
};

static Output* s_instance;

static void NewOutputCallback( struct wl_listener* listener, void* data )
{
    wlr_log( WLR_INFO, "New output" );
}

Output::Output( const Backend& backend )
    : m_layout( wlr_output_layout_create() )
{
    CheckPanic( m_layout, "Failed to create wlr_output_layout!" );

    CheckPanic( !s_instance, "Creating a second instance of Output!" );
    s_instance = this;

    wl_list_init( &m_outputs );
    m_newOutput.notify = NewOutputCallback;
    wl_signal_add( &backend.Get()->events.new_output, &m_newOutput );
}

Output::~Output()
{
    wlr_output_layout_destroy( m_layout );
    s_instance = nullptr;
}
