#version 450

in vec4 gl_FragCoord;

layout(location = 0) out vec4 outColor;

void main() {
    float dt = dot( gl_FragCoord.xy * 0.001, vec2( 12.9898, 78.233 ) );
    float noise = fract( sin( dt ) * 43758.5453 );

    const float c = pow( 0.1125, 2.2 ) + noise / 255.0;
    outColor = vec4( c, c, c, 1.0 );
}
