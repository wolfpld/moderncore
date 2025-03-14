#version 450
#include "Pq.frag"

in vec4 gl_FragCoord;

layout(location = 0) in vec2 outTexCoord;
layout(location = 0) out vec4 outColor;
layout(binding = 0) uniform sampler2D tex;

layout(push_constant) uniform PushConstants {
    layout(offset = 8) float div;
};

void main()
{
    float texDelta = 1.0 / textureSize( tex, 0 ).x;
    float dx = dFdx(outTexCoord.x);
    float dy = dFdy(outTexCoord.y);

    const float mul = min( 1.0, ( dx - texDelta ) / ( texDelta * 0.25 ) );
    const vec2 off = vec2( 0.125, 0.375 ) * mul;

    vec4 acc = vec4(0.0);
    acc += texture(tex, outTexCoord + vec2( dx * off.x,  dy * off.y));
    acc += texture(tex, outTexCoord + vec2(-dx * off.x, -dy * off.y));
    acc += texture(tex, outTexCoord + vec2( dx * off.y, -dy * off.x));
    acc += texture(tex, outTexCoord + vec2(-dx * off.y,  dy * off.x));
    outColor = acc * 0.25;

    if( outColor.a < 1.0 )
    {
        const float lo = pow( 0.2, 2.2 );
        const float hi = pow( 0.3, 2.2 );
        const vec4 darkColor = vec4( lo, lo, lo, 1.0 );
        const vec4 lightColor = vec4( hi, hi, hi, 1.0 );
        vec4 checkerboard = ( ( int( gl_FragCoord.x * div ) + int( gl_FragCoord.y * div ) ) & 1 ) == 0 ? darkColor : lightColor;
        outColor = vec4( Pq( mix( outColor, checkerboard, 1.0 - outColor.a ).rgb ), 1.0 );
    }
    else
    {
        outColor = vec4( Pq( outColor.rgb ), 1.0 );
    }
}
