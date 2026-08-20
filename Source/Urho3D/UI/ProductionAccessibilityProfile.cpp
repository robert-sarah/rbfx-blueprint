// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#include "ProductionAccessibilityProfile.h"

#include <algorithm>
#include <cmath>

namespace Urho3D
{

ProductionAccessibilityValidation ProductionAccessibilityProfile::Validate(
    const ProductionAccessibilitySettings& settings)
{
    ProductionAccessibilityValidation result;
    result.valid = true;
    if (!std::isfinite(settings.textScale) || settings.textScale < 0.5f || settings.textScale > 3.0f)
        result.errors.emplace_back("textScale must be finite and within [0.5, 3.0]");
    if (!std::isfinite(settings.interfaceScale) || settings.interfaceScale < 0.5f || settings.interfaceScale > 3.0f)
        result.errors.emplace_back("interfaceScale must be finite and within [0.5, 3.0]");
    result.valid = result.errors.empty();
    return result;
}

float ProductionAccessibilityProfile::ScaleText(float baseSize, const ProductionAccessibilitySettings& settings)
{
    return std::max(baseSize, 0.0f) * std::clamp(settings.textScale, 0.5f, 3.0f);
}

float ProductionAccessibilityProfile::ScaleMotionDuration(float baseDuration,
    const ProductionAccessibilitySettings& settings)
{
    const float duration = std::max(baseDuration, 0.0f);
    return settings.reducedMotion ? 0.0f : duration;
}

} // namespace Urho3D
