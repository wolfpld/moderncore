#version 450

layout(location = 0) in vec2 inPosition;
layout(location = 0) out vec2 outTexCoord;

layout(push_constant) uniform PushConstants {
    vec2 mul;
};

void main() {
    gl_Position = vec4( inPosition.x * mul.x, inPosition.y * mul.y, 0.0, 1.0);
    outTexCoord = inPosition * 0.5 + 0.5;
}
