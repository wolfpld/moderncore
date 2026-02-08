#version 450
#include "Pq.frag"

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    layout(offset = 8) float div;
    float offset;
};

void main() {
    float wave = sin( ( gl_FragCoord.x + gl_FragCoord.y - offset ) * div );
    float k = 0.5;
    float ss = smoothstep( -k, k, wave );
    vec3 color = Pq( vec3( pow( ss, 2.2 ) * 2.03 ) );
    outColor = vec4( color, 1.0 );
}
