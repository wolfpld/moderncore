#pragma once

#include <stdint.h>
#include <string>
#include <wayland-client.h>

#include "../../util/NoCopy.hpp"

class WaylandOutput
{
public:
    explicit WaylandOutput( wl_output* output, uint32_t name );
    ~WaylandOutput();

    NoCopy( WaylandOutput );

    [[nodiscard]] int32_t GetScale() const { return m_scale; }
    [[nodiscard]] wl_output* GetOutput() const { return m_output; }

private:
    void Geometry( wl_output* output, int32_t x, int32_t y, int32_t phys_w, int32_t phys_h, int32_t subpixel, const char* make, const char* model, int32_t transform );
    void Mode( wl_output* output, uint32_t flags, int32_t width, int32_t height, int32_t refresh );
    void Done( wl_output* output );
    void Scale( wl_output* output, int32_t scale );
    void Name( wl_output* output, const char* name );
    void Description( wl_output* output, const char* description );

    wl_output* m_output;
    uint32_t m_waylandName;
    int32_t m_scale;

    std::string m_name;
    std::string m_description;
};
