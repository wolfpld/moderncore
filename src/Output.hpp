#ifndef __OUTPUT_HPP__
#define __OUTPUT_HPP__

#include <wayland-server-core.h>

extern "C" {
    struct wlr_output;
    struct wlr_output_layout;
};

class Backend;

class Output
{
public:
    explicit Output( const Backend& backend );
    ~Output();

    struct wlr_output_layout* GetLayout() const { return m_layout; }

    void NewOutput( struct wlr_output* output );

private:
    struct wlr_output_layout* m_layout;
    struct wl_list m_outputs;
    struct wl_listener m_newOutput;
};

#endif
