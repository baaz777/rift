#pragma once

#include "SupportSurface.hpp"

/**
 * @struct Elevation
 * @brief Visual interpolation and committed logical support.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Entities
 *
 * offset is render-only and can lag plane. plane and surface form one SupportState;
 * write them together through CharacterKinematics::CommitSupport.
 */
struct Elevation
{
    float offset{0.0f};  ///< Smoothed visual Y shift in pixels; render-only, lags `plane`.
    float target{0.0f};
    float start{0.0f};
    float progress{1.0f};  ///< Interpolation progress; 0 = start, 1 = settled.
    int plane{0};          ///< Committed support height in pixels; 0 = ground. Collision reads it.
    SupportSurface surface{SupportSurface::Ground};
};
