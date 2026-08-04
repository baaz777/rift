#version 450

// Sample texture, vertex tint, and ambient; solid geometry uses the white texture.

layout(location = 0) in vec2 TexCoord;
layout(location = 1) in vec4 VertexColor;

layout(location = 0) out vec4 FragColor;

#ifdef USE_VULKAN

layout(binding = 0) uniform sampler2D sprite;

layout(push_constant) uniform PushConstants
{
    layout(offset = 0) mat4 viewProjection;
    layout(offset = 64) vec3 ambientColor;
    layout(offset = 76) float alphaCutoff;
}
pc;

#define AMBIENT_COLOR (pc.ambientColor)
#define ALPHA_CUTOFF (pc.alphaCutoff)

#else

uniform sampler2D sprite;
uniform vec3 ambientColor;
// Depth-writing draws use alpha cutoff 0.5; other passes use 1/255.
// Discarded fragments write neither color nor depth.
uniform float alphaCutoff;

#define AMBIENT_COLOR ambientColor
#define ALPHA_CUTOFF alphaCutoff

#endif

void main()
{
    vec4 texColor = texture(sprite, TexCoord);
    vec4 result = texColor * VertexColor;

    if (result.a < ALPHA_CUTOFF)
    {
        discard;
    }

    FragColor = vec4(result.rgb * AMBIENT_COLOR, result.a);
}
