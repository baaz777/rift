#version 450

// OpenGL bloom gate uses hsv saturation, independent of brightness.
// Karis weight over / (1 + over) scales the source color; neutral pixels contribute nothing.
//
//   scene (RGB16F) --BloomPrefilter--> mip0        mip0 = half scene resolution
//
//   downsample (BloomDownsample.frag), destination CLEARED each pass:
//     mip0 --> mip1 --> mip2 --> ... --> mipN-1    each level halves again
//
//   upsample (BloomUpsample.frag), GL_ONE/GL_ONE additive, destination NOT
//   cleared - it already holds what the downsample wrote there:
//     mipN-1 --+--> mipN-2 --+--> ... --+--> mip0
//
//   mip0 --> uBloom in PostFXComposite.frag
//
// uSrcTexelSize is the reciprocal size of the sampled mip. never clear upsample destinations.

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 FragColor;

layout(binding = 0) uniform sampler2D uScene;

// Fixed by ambience::BLOOM_SATURATION_THRESHOLD.
uniform float uSatThreshold;

void main()
{
    vec3 col = texture(uScene, vUV).rgb;

    // Matches PostFXParams::HsvSaturation.
    float maxC = max(max(col.r, col.g), col.b);
    float minC = min(min(col.r, col.g), col.b);
    float sat = (maxC > 1e-4) ? (maxC - minC) / maxC : 0.0;

    // Matches KarisBloomChromaWeight; zero at or below threshold.
    float over = max(sat - uSatThreshold, 0.0);
    float weight = over / (1.0 + over);

    FragColor = vec4(col * weight, 1.0);
}
