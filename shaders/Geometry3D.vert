#version 450

// Scene = (worldX, height, worldY), in world-pixel units with +Y up.
layout(location = 0) in vec3 aPos;

// Texture coordinates (UVs), already resolved to atlas space on the CPU.
layout(location = 1) in vec2 aTexCoord;

// Per-vertex RGBA tint. carries the sprite/tile colour and alpha so that a
// single batch can mix differently tinted quads without a uniform change.
layout(location = 2) in vec4 aColor;

layout(location = 0) out vec2 TexCoord;
layout(location = 1) out vec4 VertexColor;

#ifdef USE_VULKAN

layout(push_constant) uniform PushConstants
{
    layout(offset = 0) mat4 viewProjection;
    layout(offset = 64) vec3 ambientColor;
    layout(offset = 76) float alphaCutoff;
}
pc;

#else

uniform mat4 viewProjection;

#endif

void main()
{
#ifdef USE_VULKAN
    gl_Position = pc.viewProjection * vec4(aPos, 1.0);
#else
    gl_Position = viewProjection * vec4(aPos, 1.0);
#endif

    TexCoord = aTexCoord;
    VertexColor = aColor;
}
