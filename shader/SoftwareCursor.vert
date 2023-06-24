#version 450

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inTexCoord;

layout(location = 0) out vec2 outTexCoord;

layout(set = 0, binding = 0) uniform ScreenSize {
    vec2 size;
} screenSize;

layout(push_constant) uniform PushConstants {
    vec2 pos;
} pushConstants;

void main() {
    gl_Position = vec4( (inPosition.x + pushConstants.pos.x) / screenSize.size.x * 2.0 - 1.0, (inPosition.y + pushConstants.pos.y) / screenSize.size.y * 2.0 - 1.0, 0.0, 1.0);
    outTexCoord = inTexCoord;
}
