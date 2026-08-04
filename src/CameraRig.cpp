#include "CameraRig.hpp"

#include "SceneMath.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace cameraRig
{
namespace
{

constexpr float EPSILON = 1e-6f;

// Horizon rays use a finite distance scaled by sceneRadius.
constexpr float HORIZON_RAY_LIMIT_SCALE = 4.0f;
}  // namespace

float ClampPitch(float pitchRadians)
{
    return std::clamp(pitchRadians, MIN_PITCH_RADIANS, MAX_PITCH_RADIANS);
}

OrbitAngles ApplyOrbitDrag(OrbitAngles current, glm::vec2 dragPixels, glm::vec2 viewportSize)
{
    const glm::vec2 size = glm::max(viewportSize, glm::vec2(1.0f));

    OrbitAngles next;
    // Negative yaw makes the ground follow a rightward drag.
    next.yawRadians = WrapYaw(current.yawRadians - DRAG_SWEEP_RADIANS * (dragPixels.x / size.x));
    // Raising pitch makes the ground follow a downward drag.
    next.pitchRadians =
        ClampPitch(current.pitchRadians + DRAG_SWEEP_RADIANS * (dragPixels.y / size.y));
    return next;
}

float WrapYaw(float yawRadians)
{
    constexpr float twoPi = 2.0f * rift::PiF;
    // std::remainder lands in [-pi, pi]; shift the -pi boundary up so the range
    // is half-open and a full turn is not two different representations.
    float wrapped = std::remainder(yawRadians, twoPi);
    if (wrapped <= -rift::PiF)
    {
        wrapped += twoPi;
    }
    return wrapped;
}

float DistanceForVisibleHeight(float visibleWorldHeight, float fovYRadians)
{
    const float halfFov = std::clamp(fovYRadians, EPSILON, rift::PiF - EPSILON) * 0.5f;
    const float tanHalf = std::max(std::tan(halfFov), EPSILON);
    return std::max(visibleWorldHeight, 0.0f) * 0.5f / tanHalf;
}

glm::vec3 EyeDirection(float yawRadians, float pitchRadians)
{
    const float cp = std::cos(pitchRadians);
    // Yaw 0 -> +Z (map south); yaw grows toward +X (map east).
    return {std::sin(yawRadians) * cp, std::sin(pitchRadians), std::cos(yawRadians) * cp};
}

glm::vec3 UpVector(float yawRadians, float pitchRadians)
{
    // Analytical cross product stays defined at the top-down pitch pi/2.
    const float sy = std::sin(yawRadians);
    const float cy = std::cos(yawRadians);
    const float sp = std::sin(pitchRadians);
    const float cp = std::cos(pitchRadians);
    return {-sy * sp, cp, -cy * sp};
}

Basis MakeBasis(const RigParams& params)
{
    Basis basis;
    basis.distance = DistanceForVisibleHeight(params.visibleWorldSize.y, params.fovYRadians);
    basis.focus = sceneMath::ToScene(params.target, params.focusHeight);
    basis.eye = basis.focus + EyeDirection(params.yawRadians, params.pitchRadians) * basis.distance;
    basis.forward = -EyeDirection(params.yawRadians, params.pitchRadians);
    basis.up = UpVector(params.yawRadians, params.pitchRadians);
    basis.right = {std::cos(params.yawRadians), 0.0f, -std::sin(params.yawRadians)};
    return basis;
}

void DepthRange(const RigParams& params, float& outNear, float& outFar)
{
    const float distance = DistanceForVisibleHeight(params.visibleWorldSize.y, params.fovYRadians);
    const float radius = std::max(params.sceneRadius, 1.0f);

    if (params.kind == ProjectionKind::Orthographic)
    {
        // No perspective divide, so depth precision is uniform and a symmetric
        // slab about the focus is fine. negative near is legal here.
        outNear = distance - radius;
        outFar = distance + radius;
        return;
    }

    // Move the near plane with the eye to retain depth precision around the map.
    outNear = std::max(0.5f, distance * 0.05f);
    outFar = distance + radius * 2.0f;
}

glm::mat4 BuildView(const RigParams& params)
{
    const Basis basis = MakeBasis(params);
    return glm::lookAt(basis.eye, basis.focus, basis.up);
}

glm::mat4 BuildProjection(const RigParams& params)
{
    float nearPlane = 0.0f;
    float farPlane = 0.0f;
    DepthRange(params, nearPlane, farPlane);

    const glm::vec2 half = glm::max(params.visibleWorldSize, glm::vec2(EPSILON)) * 0.5f;

    if (params.kind == ProjectionKind::Orthographic)
    {
        // visibleWorldSize already includes the viewport aspect ratio.
        return glm::ortho(-half.x, half.x, -half.y, half.y, nearPlane, farPlane);
    }

    const float aspect = half.x / half.y;
    return glm::perspective(params.fovYRadians, aspect, nearPlane, farPlane);
}

glm::mat4 BuildViewProjection(const RigParams& params)
{
    return BuildProjection(params) * BuildView(params);
}

Ray ScreenToRay(glm::vec2 pixel, glm::vec2 viewportSize, const glm::mat4& invViewProj)
{
    const glm::vec2 size = glm::max(viewportSize, glm::vec2(1.0f));

    // Framebuffer pixels are Y-down; NDC is Y-up.
    const float ndcX = 2.0f * (pixel.x / size.x) - 1.0f;
    const float ndcY = 1.0f - 2.0f * (pixel.y / size.y);

    const glm::vec4 nearClip = invViewProj * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
    const glm::vec4 farClip = invViewProj * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);

    Ray ray;
    if (std::abs(nearClip.w) < EPSILON || std::abs(farClip.w) < EPSILON)
    {
        return ray;
    }

    const glm::vec3 nearPoint = glm::vec3(nearClip) / nearClip.w;
    const glm::vec3 farPoint = glm::vec3(farClip) / farClip.w;

    const glm::vec3 delta = farPoint - nearPoint;
    const float length = glm::length(delta);
    if (length < EPSILON)
    {
        return ray;
    }

    ray.origin = nearPoint;
    ray.direction = delta / length;
    return ray;
}

std::optional<glm::vec2> IntersectGroundPlane(const Ray& ray, float planeHeight)
{
    if (std::abs(ray.direction.y) < EPSILON)
    {
        return std::nullopt;
    }

    const float t = (planeHeight - ray.origin.y) / ray.direction.y;
    if (t < 0.0f)
    {
        return std::nullopt;
    }

    return sceneMath::ToWorld(ray.origin + ray.direction * t);
}

std::optional<glm::vec2> ScreenToGround(glm::vec2 pixel,
                                        glm::vec2 viewportSize,
                                        const glm::mat4& invViewProj,
                                        float planeHeight)
{
    return IntersectGroundPlane(ScreenToRay(pixel, viewportSize, invViewProj), planeHeight);
}

std::optional<glm::vec2> WorldToScreen(glm::vec3 scenePoint,
                                       const glm::mat4& viewProj,
                                       glm::vec2 viewportSize)
{
    const glm::vec4 clip = viewProj * glm::vec4(scenePoint, 1.0f);
    if (clip.w <= EPSILON)
    {
        return std::nullopt;
    }

    const glm::vec3 ndc = glm::vec3(clip) / clip.w;
    const glm::vec2 size = glm::max(viewportSize, glm::vec2(1.0f));
    return glm::vec2((ndc.x * 0.5f + 0.5f) * size.x, (0.5f - ndc.y * 0.5f) * size.y);
}

GroundBounds GroundFootprintAabb(const RigParams& params, float planeHeight)
{
    const glm::mat4 invViewProj = glm::inverse(BuildViewProjection(params));

    // Corner rays depend on NDC fractions, so a unit viewport suffices.
    const glm::vec2 viewport{1.0f, 1.0f};
    const std::array<glm::vec2, 4> corners{
        glm::vec2{0.0f, 0.0f},
        glm::vec2{1.0f, 0.0f},
        glm::vec2{1.0f, 1.0f},
        glm::vec2{0.0f, 1.0f},
    };

    const float rayLimit = std::max(params.sceneRadius, 1.0f) * HORIZON_RAY_LIMIT_SCALE;

    GroundBounds bounds;
    // Seed with the focus so the box is never empty or inverted, even if every
    // corner ray misses.
    bounds.min = params.target;
    bounds.max = params.target;

    for (const glm::vec2& corner : corners)
    {
        const Ray ray = ScreenToRay(corner, viewport, invViewProj);
        glm::vec2 hit;

        if (const std::optional<glm::vec2> intersection = IntersectGroundPlane(ray, planeHeight))
        {
            hit = *intersection;
        }
        else
        {
            // Bound horizon misses and mark the footprint incomplete.
            bounds.complete = false;
            hit = sceneMath::ToWorld(ray.origin + ray.direction * rayLimit);
        }

        bounds.min = glm::min(bounds.min, hit);
        bounds.max = glm::max(bounds.max, hit);
    }

    return bounds;
}

void ApplyPreset(RigParams& params, Preset preset)
{
    switch (preset)
    {
        case Preset::Classic:
            params.yawRadians = 0.0f;
            params.pitchRadians = MAX_PITCH_RADIANS;
            params.fovYRadians = DS_FOV_RADIANS;
            params.kind = ProjectionKind::Orthographic;
            break;

        case Preset::DS:
            params.yawRadians = 0.0f;
            params.pitchRadians = DS_PITCH_RADIANS;
            params.fovYRadians = DS_FOV_RADIANS;
            params.kind = ProjectionKind::Perspective;
            break;

        case Preset::Free:

            params.kind = ProjectionKind::Perspective;
            params.pitchRadians = ClampPitch(params.pitchRadians);
            params.yawRadians = WrapYaw(params.yawRadians);
            break;
    }
}

glm::vec2 ClampFocusToMap(glm::vec2 focus, glm::vec2 mapPixelSize)
{
    const glm::vec2 limit = glm::max(mapPixelSize, glm::vec2(0.0f));
    return {std::clamp(focus.x, 0.0f, limit.x), std::clamp(focus.y, 0.0f, limit.y)};
}

}  // namespace cameraRig
