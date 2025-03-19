#version 450

layout(location = 0) in vec2 outTexCoord;
layout(location = 0) out vec4 outColor;
layout(binding = 0) uniform sampler2D tex;

layout(push_constant) uniform PushConstants {
    layout(offset = 8) float div;
};

vec4 filteredNearest(sampler2D tex, vec2 coord)
{
    vec2 texSize = vec2( textureSize( tex, 0 ) );
    vec2 delta = vec2(
        dFdx( coord.x ) * texSize.x,
        dFdy( coord.y ) * texSize.y
    );
    coord = coord * texSize + 0.5;
    vec2 i = floor( coord );
    vec2 f = fract( coord );
    coord = i + smoothstep( 0.5 - delta, 0.5 + delta, f );
    coord = ( coord - 0.5 ) / texSize;
    return texture( tex, coord );
}

void main() {
    outColor = filteredNearest(tex, outTexCoord);

    if( outColor.a < 1.0 )
    {
        const float lo = pow( 0.2, 2.2 );
        const float hi = pow( 0.3, 2.2 );
        const vec4 darkColor = vec4( lo, lo, lo, 1.0 );
        const vec4 lightColor = vec4( hi, hi, hi, 1.0 );
        vec4 checkerboard = ( ( int( gl_FragCoord.x * div ) + int( gl_FragCoord.y * div ) ) & 1 ) == 0 ? darkColor : lightColor;
        outColor = vec4( mix( outColor, checkerboard, 1.0 - outColor.a ).rgb, 1.0 );
    }
}
