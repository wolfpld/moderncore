#version 450

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inTexCoord;

layout(location = 0) out vec2 outTexCoord;

void main() {
    gl_Position = vec4(inPosition.x / 1650.0 * 2.0 - 1.0, inPosition.y / 1050 * 2.0 - 1.0, 0.0, 1.0);
    outTexCoord = inTexCoord;
}
