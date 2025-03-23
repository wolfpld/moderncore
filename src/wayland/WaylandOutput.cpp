#include "WaylandOutput.hpp"
#include "util/Invoke.hpp"
#include "util/Logs.hpp"

WaylandOutput::WaylandOutput( wl_output* output, uint32_t id, wp_color_manager_v1* colorManager )
    : m_output( output )
    , m_outputColor( wp_color_manager_v1_get_output( colorManager, output ) )
    , m_imageDescription( nullptr )
    , m_imageDescriptionInfo( nullptr )
    , m_id( id )
    , m_maxLuminance( -1 )
{
    static constexpr wl_output_listener outputListener = {
        .geometry = Method( Geometry ),
        .mode = Method( Mode ),
        .done = Method( Done ),
        .scale = Method( Scale ),
        .name = Method( Name ),
        .description = Method( Description )
    };
    wl_output_add_listener( m_output, &outputListener, this );

    static constexpr wp_color_management_output_v1_listener colorListener = {
        .image_description_changed = Method( ImageDescriptionChanged )
    };
    wp_color_management_output_v1_add_listener( m_outputColor, &colorListener, this );

    GetImageDescription();
}

WaylandOutput::~WaylandOutput()
{
    mclog( LogLevel::Debug, "Display %s disconnected from %s", m_description.c_str(), m_name.c_str() );

    if( m_imageDescriptionInfo ) wp_image_description_info_v1_destroy( m_imageDescriptionInfo );
    if( m_imageDescription ) wp_image_description_v1_destroy( m_imageDescription );
    wp_color_management_output_v1_destroy( m_outputColor );
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
    if( m_initDone ) return;
    m_initDone = true;

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

void WaylandOutput::ImageDescriptionChanged( wp_color_management_output_v1* output )
{
    GetImageDescription();
}

void WaylandOutput::GetImageDescription()
{
    if( m_imageDescription ) wp_image_description_v1_destroy( m_imageDescription );
    m_imageDescription = wp_color_management_output_v1_get_image_description( m_outputColor );

    static constexpr wp_image_description_v1_listener listener = {
        .failed = Method( ImageDescriptionFailed ),
        .ready = Method( ImageDescriptionReady )
    };
    wp_image_description_v1_add_listener( m_imageDescription, &listener, this );
}

void WaylandOutput::ImageDescriptionFailed( wp_image_description_v1* desc, uint32_t cause, const char* msg )
{
    CheckPanic( m_imageDescription == desc, "Invalid image description" );
    mclog( LogLevel::Error, "Failed to get output color description: %s", msg );
    wp_image_description_v1_destroy( m_imageDescription );
    m_imageDescription = nullptr;
}

void WaylandOutput::ImageDescriptionReady( wp_image_description_v1* desc, uint32_t identity )
{
    CheckPanic( m_imageDescription == desc, "Invalid image description" );
    m_imageDescriptionInfo = wp_image_description_v1_get_information( m_imageDescription );

    static constexpr wp_image_description_info_v1_listener listener = {
        .done = Method( ColorDone ),
        .icc_file = Method( ColorFile ),
        .primaries = Method( ColorPrimaries ),
        .primaries_named = Method( ColorPrimariesNamed ),
        .tf_power = Method( ColorTfPower ),
        .tf_named = Method( ColorTfNamed ),
        .luminances = Method( ColorLuminances ),
        .target_primaries = Method( ColorTargetPrimaries ),
        .target_luminance = Method( ColorTargetLuminance ),
        .target_max_cll = Method( ColorTargetMaxCll ),
        .target_max_fall = Method( ColorTargetMaxFall ),
    };
    wp_image_description_info_v1_add_listener( m_imageDescriptionInfo, &listener, this );
}

void WaylandOutput::ColorDone( wp_image_description_info_v1* info )
{
    CheckPanic( m_imageDescriptionInfo == info, "Invalid image description info" );
    wp_image_description_info_v1_destroy( m_imageDescriptionInfo );
    m_imageDescriptionInfo = nullptr;
    wp_image_description_v1_destroy( m_imageDescription );
    m_imageDescription = nullptr;

    mclog( LogLevel::Debug, "Display %s max luminance: %d", m_description.c_str(), m_maxLuminance );
}

void WaylandOutput::ColorFile( wp_image_description_info_v1* info, int32_t icc, uint32_t size )
{
}

void WaylandOutput::ColorPrimaries( wp_image_description_info_v1* info, int32_t rx, int32_t ry, int32_t gx, int32_t gy, int32_t bx, int32_t by, int32_t wx, int32_t wy )
{
}

void WaylandOutput::ColorPrimariesNamed( wp_image_description_info_v1* info, uint32_t primaries )
{
}

void WaylandOutput::ColorTfPower( wp_image_description_info_v1* info, uint32_t eexp )
{
}

void WaylandOutput::ColorTfNamed( wp_image_description_info_v1* info, uint32_t tf )
{
}

void WaylandOutput::ColorLuminances( wp_image_description_info_v1* info, uint32_t min, uint32_t max, uint32_t reference )
{
}

void WaylandOutput::ColorTargetPrimaries( wp_image_description_info_v1* info, int32_t rx, int32_t ry, int32_t gx, int32_t gy, int32_t bx, int32_t by, int32_t wx, int32_t wy )
{
}

void WaylandOutput::ColorTargetLuminance( wp_image_description_info_v1* info, uint32_t min, uint32_t max )
{
    m_maxLuminance = max;
}

void WaylandOutput::ColorTargetMaxCll( wp_image_description_info_v1* info, uint32_t maxCll )
{
}

void WaylandOutput::ColorTargetMaxFall( wp_image_description_info_v1* info, uint32_t maxFall )
{
}
