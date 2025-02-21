#version 450

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inTexCoord;

layout(location = 0) out vec2 outTexCoord;

layout(push_constant) uniform PushConstants {
    vec2 screenSize;
};

void main() {
    gl_Position = vec4( inPosition.x / screenSize.x * 2.0 - 1.0, inPosition.y / screenSize.y * 2.0 - 1.0, 0.0, 1.0 );
    outTexCoord = inTexCoord;
}
