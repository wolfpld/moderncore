#version 450

in vec4 gl_FragCoord;

layout(location = 0) out vec4 outColor;

void main() {
    const float div = 8.0;
    const float lo = pow( 0.2, 2.2 );
    const float hi = pow( 0.3, 2.2 );
    const vec4 darkColor = vec4( lo, lo, lo, 1.0 );
    const vec4 lightColor = vec4( hi, hi, hi, 1.0 );
    outColor = ( ( int( gl_FragCoord.x / div ) + int( gl_FragCoord.y / div ) ) & 1 ) == 0 ? darkColor : lightColor;
}
