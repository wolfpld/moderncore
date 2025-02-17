#version 450

layout(location = 0) in vec2 inPosition;
layout(location = 0) out vec2 outTexCoord;

layout(push_constant) uniform PushConstants {
    vec2 mul;
    float time;
};

void main() {
    float c = cos( time );
    float s = sin( time );
    mat2 rot = mat2( c, s, -s, c );
    vec2 pos = rot * inPosition;

    gl_Position = vec4( pos.x * mul.x, pos.y * mul.y, 0.0, 1.0);
    outTexCoord = inPosition * 0.5 + 0.5;
}
