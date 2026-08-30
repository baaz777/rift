#version 450

// OpenGL scene composition order:
//
//   0. Master gate (uPostFXEnabled) - when 0, return the raw scene texel
//   1. Sample scene with chromatic aberration per-channel offset (3 fetches)
//   2. Add chroma-only bloom (zero net luminance contribution)
//   3. LGG grading split (shadows / midtones / highlights)
//   4. Saturation pump (after grading, before vignette)
//   5. Vignette + edge desaturation
//   6. Grain (luminance-modulated, slight chroma)
//   7. Tonemap (soft-shoulder)
//
// PostFXParams supplies per-frame controls. ambience constants supply lens geometry,
// edge desaturation, grain chroma mix, and tonemap knee.

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 FragColor;

layout(binding = 0) uniform sampler2D uScene;
layout(binding = 1) uniform sampler2D uBloom;

// Bloom + grading
uniform float uBloomIntensity;
uniform vec3 uLift;
uniform vec3 uGamma;
uniform vec3 uGain;
uniform float uSaturation;

// Lens character
uniform float uCAStrength;
uniform float uVignetteIntensity;
uniform float uVignetteInnerR;
uniform float uVignetteOuterR;
uniform float uVignetteAspectY;
uniform float uEdgeDesat;

// Grain
uniform float uGrainIntensity;
uniform float uGrainChromaMix;
uniform float uTime;

// Tonemap
uniform float uTonemapKnee;

uniform int uPostFXEnabled;

const vec3 LUMA = vec3(0.2126, 0.7152, 0.0722);

float hash21(vec2 p)
{
    // Cheap 2D->1D hash. used only for grain noise.
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

// Sample RGB at separate radial offsets; the centre remains aligned.
vec3 sampleSceneCA(vec2 uv, float caAmount)
{
    vec2 caDir = uv - 0.5;
    vec3 col;
    col.r = texture(uScene, uv + caDir * caAmount).r;
    col.g = texture(uScene, uv).g;
    col.b = texture(uScene, uv - caDir * caAmount).b;
    return col;
}

// Asc cdl: out = pow(in * gain + lift, 1.0 / gamma).
vec3 applyLGG(vec3 c, vec3 lift, vec3 gamma, vec3 gain)
{
    vec3 step = max(c * gain + lift, vec3(0.0));
    return pow(step, vec3(1.0) / gamma);
}

// Saturation follows grading and precedes edge desaturation; 1 is identity.
vec3 applySaturation(vec3 c, float s)
{
    float lum = dot(c, LUMA);
    return mix(vec3(lum), c, s);
}

float computeVignetteFactor(vec2 uv, float aspectY, float innerR, float outerR)
{
    vec2 uvCentered = (uv - 0.5) * vec2(1.0, aspectY);
    // sqrt(2) maps unscaled UV corners to radius 1.
    float r = length(uvCentered) * 1.41421356;
    return smoothstep(innerR, outerR, r);
}

vec3 applyVignette(vec3 c, float vig, float intensity, float edgeDesat)
{
    vec3 darkened = c * mix(1.0, 1.0 - intensity, vig);
    float lum = dot(darkened, LUMA);
    return mix(darkened, vec3(lum), vig * edgeDesat);
}

vec3 applyGrain(vec3 c, float time, float intensity, float chromaMix)
{
    float lum = dot(c, LUMA);
    // The unclamped luminance parabola is negative above 1; HDR highlights invert and amplify
    // grain.
    float lumMod = 4.0 * lum * (1.0 - lum);

    // 2x2 pixel tiles (gl_FragCoord is in pixels).
    vec2 grainCoord = floor(gl_FragCoord.xy * 0.5) + time * 60.0;
    vec3 grain;
    grain.r = hash21(grainCoord) - 0.5;
    grain.g = hash21(grainCoord + 17.13) - 0.5;
    grain.b = hash21(grainCoord + 91.41) - 0.5;
    vec3 grainLuma = vec3(dot(grain, vec3(1.0 / 3.0)));
    vec3 grainFinal = mix(grainLuma, grain, chromaMix);

    return c + grainFinal * intensity * lumMod;
}

// Below the knee, preserve input; above it, approach 1 asymptotically.
// min(c, above) selects the branch continuously at the knee.
vec3 softShoulderTonemap(vec3 c, float knee)
{
    vec3 over = max(c - knee, vec3(0.0));
    float headroom = max(1.0 - knee, 1e-4);
    vec3 above = knee + over * headroom / (over + headroom);
    return min(c, above);
}

void main()
{

    if (uPostFXEnabled == 0)
    {
        FragColor = vec4(texture(uScene, vUV).rgb, 1.0);
        return;
    }

    vec3 col = sampleSceneCA(vUV, uCAStrength);

    // Remove bloom luminance before adding chroma: bChroma = b - vec3(dot(b, LUMA)).
    // A pure red bloom contributes (0.79, -0.21, -0.21); clamp the sum nonnegative before pow
    // grading.
    vec3 b = texture(uBloom, vUV).rgb;
    vec3 bChroma = b - vec3(dot(b, LUMA));
    col = max(col + bChroma * uBloomIntensity, vec3(0.0));

    // 3. lgg grading (shadows / midtones / highlights).
    col = applyLGG(col, uLift, uGamma, uGain);

    // 4. saturation pump.
    col = applySaturation(col, uSaturation);

    // 5. vignette + edge desaturation.
    float vig = computeVignetteFactor(vUV, uVignetteAspectY, uVignetteInnerR, uVignetteOuterR);
    col = applyVignette(col, vig, uVignetteIntensity, uEdgeDesat);

    // 6. grain - added before tonemap so it feels camera-captured, not overlaid.
    col = applyGrain(col, uTime, uGrainIntensity, uGrainChromaMix);

    // 7. tonemap last so the curve sees the fully-composited HDR image.
    col = softShoulderTonemap(col, uTonemapKnee);

    FragColor = vec4(col, 1.0);
}
