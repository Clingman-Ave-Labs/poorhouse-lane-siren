#pragma once

#include <cmath>

namespace dsp {

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
constexpr float PI_F = static_cast<float>(M_PI);

// Replace NaN / Inf with zero — use in every feedback path
inline float sanitize(float x)
{
    return std::isfinite(x) ? x : 0.0f;
}

// Fast tanh approximation (Pade, accurate for |x| < ~3)
inline float fast_tanh(float x)
{
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

} // namespace dsp
