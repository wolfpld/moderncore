#version 450
#include "Pq.frag"

layout(location = 0) in vec2 outTexCoord;
layout(location = 0) out vec4 outColor;
layout(binding = 0) uniform sampler2D tex;

layout(push_constant) uniform PushConstants {
    layout(offset = 8) float div;
};

void main() {
    vec4 color = texture(tex, outTexCoord);

    if( color.a < 1.0 )
    {
        const float lo = pow( 0.2, 2.2 );
        const float hi = pow( 0.3, 2.2 );
        const vec4 darkColor = vec4( lo, lo, lo, 1.0 );
        const vec4 lightColor = vec4( hi, hi, hi, 1.0 );
        vec4 checkerboard = ( ( int( gl_FragCoord.x * div ) + int( gl_FragCoord.y * div ) ) & 1 ) == 0 ? darkColor : lightColor;
        outColor = vec4( Pq( mix( color, checkerboard, 1.0 - color.a ).rgb ), 1.0 );
    }
    else
    {
        outColor = vec4( Pq( color.rgb ), 1.0 );
    }
}
