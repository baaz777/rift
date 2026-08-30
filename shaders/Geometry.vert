#version 450

// CPU vertices already include camera translation, scale, and rotation.
layout(location = 0) in vec2 aPos;

// Texture coordinates (UVs) for this vertex (0..1 range typically).
layout(location = 1) in vec2 aTexCoord;

// Only OpenGL binds aColor for rect, particle, and text batches.
// Vulkan leaves VertexColor undefined; its fragment branch must not read it.
layout(location = 2) in vec4 aColor;  // RGBA

// UV coordinates forwarded to the fragment shader for texture sampling.
layout(location = 0) out vec2 TexCoord;

// Per-vertex color forwarded to fragment shader (interpolated across the face).
layout(location = 1) out vec4 VertexColor;

#ifdef USE_VULKAN

// Preserve the leading 160 bytes of CombinedPushConstants.
// ambientColor and spriteAlpha occupy the fragment-only tail of the 176-byte block.
layout(push_constant) uniform PushConstants
{
    layout(offset = 0) mat4 projection;       // Orthographic screen-to-clip matrix
    layout(offset = 64) mat4 model;           // Identity except on the Vulkan glyph path
    layout(offset = 128) vec3 spriteColor;    // (not used here) tint for fragment shader
    layout(offset = 140) float useColorOnly;  // (not used here) mode switch for fragment shader
    layout(offset = 144) vec4 colorOnly;      // (not used here) uniform solid color
}
pc;

#else

// OpenGL batches use identity model; Vulkan glyphs can supply a model transform.
uniform mat4 projection;
uniform mat4 model;

#endif

void main()
{
    // 2D depth follows submission order; model is identity outside Vulkan glyph draws.
#ifdef USE_VULKAN
    gl_Position = pc.projection * pc.model * vec4(aPos, 0.0, 1.0);
#else
    gl_Position = projection * model * vec4(aPos, 0.0, 1.0);
#endif

    TexCoord = aTexCoord;
    VertexColor = aColor;
}
