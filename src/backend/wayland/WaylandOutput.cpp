#include <stdio.h>

#include "WaylandMethod.hpp"
#include "WaylandOutput.hpp"

WaylandOutput::WaylandOutput( wl_output *output )
    : m_output( output )
    , m_scale( 1 )
{
    static constexpr struct wl_output_listener outputListener = {
        .geometry = Method( WaylandOutput, OutputGeometry ),
        .mode = Method( WaylandOutput, OutputMode ),
        .done = Method( WaylandOutput, OutputDone ),
        .scale = Method( WaylandOutput, OutputScale ),
        .name = Method( WaylandOutput, OutputName ),
        .description = Method( WaylandOutput, OutputDescription )
    };

    wl_output_add_listener( m_output, &outputListener, this );
}

WaylandOutput::~WaylandOutput()
{
    wl_output_destroy( m_output );
}

void WaylandOutput::OutputGeometry( wl_output* output, int32_t x, int32_t y, int32_t phys_w, int32_t phys_h, int32_t subpixel, const char* make, const char* model, int32_t transform )
{
}

void WaylandOutput::OutputMode( wl_output* output, uint32_t flags, int32_t width, int32_t height, int32_t refresh )
{
}

void WaylandOutput::OutputDone( wl_output* output )
{
}

void WaylandOutput::OutputScale( wl_output* output, int32_t scale )
{
    m_scale = scale;
}

void WaylandOutput::OutputName( wl_output* output, const char* name )
{
}

void WaylandOutput::OutputDescription( wl_output* output, const char* description )
{
}
