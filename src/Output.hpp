#ifndef __OUTPUT_HPP__
#define __OUTPUT_HPP__

#include <wayland-server-core.h>

#include "Listener.hpp"

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

    wlr_output_layout* GetLayout() const { return m_layout; }

private:
    void NewOutput( wlr_output* output );

    wlr_output_layout* m_layout;
    wl_list m_outputs;
    Listener<wlr_output> m_newOutput;
};

#endif
