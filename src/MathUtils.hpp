#pragma once

#include <algorithm>
#include <cmath>

/**
 * @brief Frame-rate-independent smoothing.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Core
 */

namespace rift
{

/**
 * @fn float rift::ExpApproachAlpha(float dt, float st, float e = 0.01f)
 * @brief Lerp alpha for a remaining-distance fraction after the settle time.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Apply the factor with `current += (target - current) * alpha`. For a fixed target, splitting
 * the elapsed time into smaller steps gives the same remaining-distance fraction.
 *
 * $$
 *   \alpha = 1 - \varepsilon^{\,\Delta t / t_{settle}}
 * $$
 *
 * @param dt Elapsed seconds; negative values clamp to zero.
 * @param st Settle time in seconds; floored at 1e-5.
 * @param e Fraction of distance remaining after st seconds; 0.01 means 99% settled.
 * @return Blend factor clamped to [0, 1].
 * @pre Inputs are finite and e is strictly between 0 and 1.
 */
inline float ExpApproachAlpha(float dt, float st, float e = 0.01f)
{
    dt = std::max(0.0f, dt);
    st = std::max(1e-5f, st);
    return std::clamp(1.0f - std::pow(e, dt / st), 0.0f, 1.0f);
}

}  // namespace rift
