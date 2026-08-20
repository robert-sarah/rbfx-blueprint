// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include "../Core/StringUtils.h"

#include <string>
#include <vector>

namespace Urho3D
{

enum class ProductionColorVisionMode
{
    Normal,
    Deuteranopia,
    Protanopia,
    Tritanopia,
    Monochrome
};

struct URHO3D_API ProductionAccessibilitySettings
{
    float textScale{1.0f};
    float interfaceScale{1.0f};
    ProductionColorVisionMode colorVision{ProductionColorVisionMode::Normal};
    bool highContrast{};
    bool reducedMotion{};
    bool controllerNavigation{true};
    bool subtitles{};
};

struct URHO3D_API ProductionAccessibilityValidation
{
    bool valid{};
    std::vector<std::string> errors;
};

/// Runtime accessibility policy consumed by UI, animation and localization systems.
class URHO3D_API ProductionAccessibilityProfile
{
public:
    static ProductionAccessibilityValidation Validate(const ProductionAccessibilitySettings& settings);
    static float ScaleText(float baseSize, const ProductionAccessibilitySettings& settings);
    static float ScaleMotionDuration(float baseDuration, const ProductionAccessibilitySettings& settings);
    static bool RequiresSubtitles(const ProductionAccessibilitySettings& settings) { return settings.subtitles; }
};

} // namespace Urho3D
