#include "WaylandOutput.hpp"
#include "util/Invoke.hpp"
#include "util/Logs.hpp"

WaylandOutput::WaylandOutput( wl_output* output, uint32_t id )
    : m_output( output )
    , m_id( id )
{
    static constexpr wl_output_listener listener = {
        .geometry = Method( Geometry ),
        .mode = Method( Mode ),
        .done = Method( Done ),
        .scale = Method( Scale ),
        .name = Method( Name ),
        .description = Method( Description )
    };

    wl_output_add_listener( m_output, &listener, this );
}

WaylandOutput::~WaylandOutput()
{
    mclog( LogLevel::Debug, "Display %s disconnected from %s", m_description.c_str(), m_name.c_str() );
    wl_output_destroy( m_output );
}

void WaylandOutput::Geometry( wl_output* output, int32_t x, int32_t y, int32_t physWidth, int32_t physHeight, int32_t subpixel, const char* make, const char* model, int32_t transform )
{
}

void WaylandOutput::Mode( wl_output* output, uint32_t flags, int32_t width, int32_t height, int32_t refresh )
{
}

void WaylandOutput::Done( wl_output* output )
{
    mclog( LogLevel::Debug, "Display %s connected at %s", m_description.c_str(), m_name.c_str() );
}

void WaylandOutput::Scale( wl_output* output, int32_t factor )
{
}

void WaylandOutput::Name( wl_output* output, const char* name )
{
    m_name = name;
}

void WaylandOutput::Description( wl_output* output, const char* desc )
{
    m_description = desc;
}
