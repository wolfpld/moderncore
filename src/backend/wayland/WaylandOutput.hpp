#ifndef __WAYLANDOUTPUT_HPP__
#define __WAYLANDOUTPUT_HPP__

#include <functional>
#include <stdint.h>
#include <wayland-client.h>

#include "../../util/NoCopy.hpp"

class WaylandOutput
{
public:
    WaylandOutput( wl_output* output, std::function<void()> onOutput );
    ~WaylandOutput();

    NoCopy( WaylandOutput );

    [[nodiscard]] int32_t GetScale() const { return m_scale; }

private:
    void OutputGeometry( wl_output* output, int32_t x, int32_t y, int32_t phys_w, int32_t phys_h, int32_t subpixel, const char* make, const char* model, int32_t transform );
    void OutputMode( wl_output* output, uint32_t flags, int32_t width, int32_t height, int32_t refresh );
    void OutputDone( wl_output* output );
    void OutputScale( wl_output* output, int32_t scale );
    void OutputName( wl_output* output, const char* name );
    void OutputDescription( wl_output* output, const char* description );

    std::function<void()> m_onOutput;
    wl_output* m_output;
    int32_t m_scale;
};

#endif
