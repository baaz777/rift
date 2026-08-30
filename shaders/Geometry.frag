#version 450

// Sprite output modes:
//
//   mode | output                       | OpenGL call site            | Vulkan
//   -----+------------------------------+-----------------------------+----------------
//     0  | tex * spriteColor * ambient  | sprite batch (FlushBatch)   | default path
//     1  | uniform colorOnly            | never selected              | DrawColoredRect
//     2  | VertexColor                  | rect batch (FlushRectBatch) | not implemented
//     3  | tex * VertexColor            | particle batch, text batch  | not implemented
//
// Vulkan binds no color attribute and supplies a boolean float mode; only 0 and 1 are valid.

// Alpha cutoff threshold for hard sprite cutouts.
const float ALPHA_CUTOFF = 0.1;

layout (location = 0) out vec4 FragColor;

layout (location = 0) in vec2 TexCoord;

// Interpolated per-vertex color (RGBA).
// Used only in one rendering mode (batched colored rects / vertex color mode).
layout (location = 1) in vec4 VertexColor;

layout (binding = 0) uniform sampler2D sprite;

#ifdef USE_VULKAN

// Explicit offsets must match CombinedPushConstants in VulkanRenderer.cpp.
layout(push_constant) uniform PushConstants {
    layout(offset = 0)   mat4 projection;     // Likely used in vertex shader
    layout(offset = 64)  mat4 model;          // Likely used in vertex shader
    layout(offset = 128) vec3 spriteColor;    // Tint color multiplied with texture RGB
    layout(offset = 140) float useColorOnly;  // Mode flag (float for Vulkan packing/alignment)
    layout(offset = 144) vec4 colorOnly;      // Solid RGBA color if "color-only" mode is active
    layout(offset = 160) vec3 ambientColor;   // Global ambient light color for day/night cycle
    layout(offset = 172) float spriteAlpha;   // Alpha multiplier for texture mode (default 1.0)
} pc;

#else

// OpenGL selects modes 0, 2, and 3; colorOnly is not uploaded.
uniform vec3 spriteColor;
uniform int  useColorOnly;
uniform vec4 colorOnly;
uniform float spriteAlpha;  // Alpha multiplier for texture mode (default 1.0)
uniform vec3 ambientColor;  // Global ambient light color for day/night cycle

#endif

void main() {

#ifdef USE_VULKAN

    if (pc.useColorOnly > 0.5) {
        // Solid color mode:
        // Ignore texture completely and output the uniform RGBA color.
        FragColor = pc.colorOnly;
    } else {
        // Texture mode:
        // Sample the sprite texture at the interpolated UV coordinates.
        vec4 texColor = texture(sprite, TexCoord);

        if (texColor.a < ALPHA_CUTOFF)
            discard;

        FragColor = vec4(pc.spriteColor * pc.ambientColor * texColor.rgb, pc.spriteAlpha * texColor.a);
    }

#else

    if (useColorOnly == 3) {
        // Textured particle mode:
        // Sample texture and multiply by per-vertex color (for batched particles).
        vec4 texColor = texture(sprite, TexCoord);
        if (texColor.a < ALPHA_CUTOFF)
            discard;
        FragColor = texColor * VertexColor;

    } else if (useColorOnly == 2) {

        FragColor = VertexColor;

    } else if (useColorOnly == 1) {
        // Uniform solid color mode:
        // Ignore texture and output a single RGBA color (e.g., for colored quads).
        FragColor = colorOnly;

    } else {
        // Texture mode (useColorOnly == 0):
        // Sample sprite texture using UVs.
        vec4 texColor = texture(sprite, TexCoord);

        // Alpha cutout (same idea as Vulkan branch):
        // Discard fragments with very low alpha to avoid drawing invisible pixels.
        if (texColor.a < ALPHA_CUTOFF)
            discard;

        // Tint the texture with spriteColor and ambientColor (for day/night cycle).
        // Multiply RGB by both colors, multiply alpha by spriteAlpha.
        FragColor = vec4(spriteColor * ambientColor * texColor.rgb, spriteAlpha * texColor.a);
    }

#endif

}
