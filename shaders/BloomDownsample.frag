#version 450

// OpenGL 13-tap bloom downsample (cod / siggraph 2014).
// Inner 2x2 samples at half-texel offsets contribute 0.5; the outer 3x3 grid contributes 0.5.

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 FragColor;

layout(binding = 0) uniform sampler2D uInput;

// Source texel size.
uniform vec2 uSrcTexelSize;

void main()
{
    vec2 t = uSrcTexelSize;

    // Inner 2x2 quad: each sample weight 0.125. total contribution: 0.5.
    vec3 inner = texture(uInput, vUV + vec2(-0.5, -0.5) * t).rgb;
    inner += texture(uInput, vUV + vec2(+0.5, -0.5) * t).rgb;
    inner += texture(uInput, vUV + vec2(-0.5, +0.5) * t).rgb;
    inner += texture(uInput, vUV + vec2(+0.5, +0.5) * t).rgb;
    inner *= 0.125;

    // Outer 3x3: each sample weight ~ 0.0556 (= 0.5 / 9). total contribution: 0.5.
    vec3 outer = texture(uInput, vUV + vec2(-1.0, -1.0) * t).rgb;
    outer += texture(uInput, vUV + vec2(+0.0, -1.0) * t).rgb;
    outer += texture(uInput, vUV + vec2(+1.0, -1.0) * t).rgb;
    outer += texture(uInput, vUV + vec2(-1.0, +0.0) * t).rgb;
    outer += texture(uInput, vUV + vec2(+0.0, +0.0) * t).rgb;
    outer += texture(uInput, vUV + vec2(+1.0, +0.0) * t).rgb;
    outer += texture(uInput, vUV + vec2(-1.0, +1.0) * t).rgb;
    outer += texture(uInput, vUV + vec2(+0.0, +1.0) * t).rgb;
    outer += texture(uInput, vUV + vec2(+1.0, +1.0) * t).rgb;
    outer *= 0.5 / 9.0;

    FragColor = vec4(inner + outer, 1.0);
}
