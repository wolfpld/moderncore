#version 450
#include "Pq.frag"

layout(location = 0) in vec2 outTexCoord;
layout(location = 0) out vec4 outColor;
layout(binding = 0) uniform sampler2D tex;

void main() {
    vec4 color = texture(tex, outTexCoord);
    outColor = vec4( Pq( color.rgb ), color.a );
}
