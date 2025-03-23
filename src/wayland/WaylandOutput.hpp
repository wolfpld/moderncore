#pragma once

#include <string>
#include <wayland-client.h>

class WaylandOutput
{
public:
    WaylandOutput( wl_output* output, uint32_t id );
    ~WaylandOutput();

    [[nodiscard]] wl_output* Output() const { return m_output; }
    [[nodiscard]] uint32_t Id() const { return m_id; }

private:
    void Geometry( wl_output* output, int32_t x, int32_t y, int32_t physWidth, int32_t physHeight, int32_t subpixel, const char* make, const char* model, int32_t transform );
    void Mode( wl_output* output, uint32_t flags, int32_t width, int32_t height, int32_t refresh );
    void Done( wl_output* output );
    void Scale( wl_output* output, int32_t factor );
    void Name( wl_output* output, const char* name );
    void Description( wl_output* output, const char* desc );

    wl_output* m_output;
    uint32_t m_id;

    std::string m_name;
    std::string m_description;

    bool m_initDone = false;
};
