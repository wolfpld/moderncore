#version 450

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    layout(offset = 8) float div;
    float offset;
};

void main() {
    float wave = sin( ( gl_FragCoord.x + gl_FragCoord.y - offset ) * div );
    float k = 0.5;
    float ss = smoothstep( -k, k, wave );
    float color = pow( ss, 2.2 );
    outColor = vec4( vec3( color ), 1.0 );
}
