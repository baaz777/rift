#version 450

// OpenGL bloom upsample uses this normalized tent kernel:
//
//     1  2  1
//     2  4  2     all divided by 16
//     1  2  1
//
// Add into the existing finer mip with GL_ONE/GL_ONE. do not clear it;
// Its downsampled content contributes to the multiscale result.

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 FragColor;

layout(binding = 0) uniform sampler2D uInput;

// Source texel size of the smaller mip.
uniform vec2 uSrcTexelSize;

void main()
{
    vec2 t = uSrcTexelSize;

    // 3x3 tent kernel - corners = 1/16, edges = 2/16, center = 4/16. sum = 16/16.
    vec3 sum = texture(uInput, vUV + vec2(-1.0, -1.0) * t).rgb * 1.0;
    sum += texture(uInput, vUV + vec2(+0.0, -1.0) * t).rgb * 2.0;
    sum += texture(uInput, vUV + vec2(+1.0, -1.0) * t).rgb * 1.0;
    sum += texture(uInput, vUV + vec2(-1.0, +0.0) * t).rgb * 2.0;
    sum += texture(uInput, vUV + vec2(+0.0, +0.0) * t).rgb * 4.0;
    sum += texture(uInput, vUV + vec2(+1.0, +0.0) * t).rgb * 2.0;
    sum += texture(uInput, vUV + vec2(-1.0, +1.0) * t).rgb * 1.0;
    sum += texture(uInput, vUV + vec2(+0.0, +1.0) * t).rgb * 2.0;
    sum += texture(uInput, vUV + vec2(+1.0, +1.0) * t).rgb * 1.0;
    sum *= (1.0 / 16.0);

    FragColor = vec4(sum, 1.0);
}
