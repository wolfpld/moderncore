#pragma once

#include <string>
#include <wayland-client.h>

#include "wayland-color-management-client-protocol.h"

class WaylandOutput
{
public:
    WaylandOutput( wl_output* output, uint32_t id, wp_color_manager_v1* colorManager );
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

    void ImageDescriptionChanged( wp_color_management_output_v1* output );
    void GetImageDescription();
    void ImageDescriptionFailed( wp_image_description_v1* desc, uint32_t cause, const char* msg );
    void ImageDescriptionReady( wp_image_description_v1* desc, uint32_t identity );

    void ColorDone( wp_image_description_info_v1* info );
    void ColorFile( wp_image_description_info_v1* info, int32_t icc, uint32_t size );
    void ColorPrimaries( wp_image_description_info_v1* info, int32_t rx, int32_t ry, int32_t gx, int32_t gy, int32_t bx, int32_t by, int32_t wx, int32_t wy );
    void ColorPrimariesNamed( wp_image_description_info_v1* info, uint32_t primaries );
    void ColorTfPower( wp_image_description_info_v1* info, uint32_t eexp );
    void ColorTfNamed( wp_image_description_info_v1* info, uint32_t tf );
    void ColorLuminances( wp_image_description_info_v1* info, uint32_t min, uint32_t max, uint32_t reference );
    void ColorTargetPrimaries( wp_image_description_info_v1* info, int32_t rx, int32_t ry, int32_t gx, int32_t gy, int32_t bx, int32_t by, int32_t wx, int32_t wy );
    void ColorTargetLuminance( wp_image_description_info_v1* info, uint32_t min, uint32_t max );
    void ColorTargetMaxCll( wp_image_description_info_v1* info, uint32_t maxCll );
    void ColorTargetMaxFall( wp_image_description_info_v1* info, uint32_t maxFall );

    wl_output* m_output;
    wp_color_management_output_v1* m_outputColor;
    wp_image_description_v1* m_imageDescription;
    wp_image_description_info_v1* m_imageDescriptionInfo;
    uint32_t m_id;

    std::string m_name;
    std::string m_description;
    int m_maxLuminance;

    bool m_initDone = false;
};
