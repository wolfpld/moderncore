#version 450

layout(location = 0) in vec2 outTexCoord;
layout(location = 0) out vec4 outColor;
layout(binding = 0) uniform sampler2D tex;

layout(push_constant) uniform PushConstants {
    layout(offset = 8) float texDelta;
};

void main() {
    float dx = dFdx(outTexCoord.x);

    if( dx * 0.9999 <= texDelta )
    {
        outColor = texture(tex, outTexCoord);
    }
    else
    {
        const float mul = min( 1.0, ( dx - texDelta ) / ( texDelta * 0.25 ) );
        const vec2 off = vec2( 0.125, 0.375 ) * mul;

        float dy = dFdy(outTexCoord.y);

        vec4 acc = vec4(0.0);
        acc += texture(tex, outTexCoord + vec2( dx * off.x,  dy * off.y));
        acc += texture(tex, outTexCoord + vec2(-dx * off.x, -dy * off.y));
        acc += texture(tex, outTexCoord + vec2( dx * off.y, -dy * off.x));
        acc += texture(tex, outTexCoord + vec2(-dx * off.y,  dy * off.x));
        outColor = acc * 0.25;
    }
}
