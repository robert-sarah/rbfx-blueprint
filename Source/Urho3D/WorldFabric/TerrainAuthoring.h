// SPDX-License-Identifier: MIT
#pragma once

#include "../Core/StringUtils.h"

#include <string>
#include <vector>

namespace Urho3D
{

enum class TerrainBrushFalloff
{
    Constant,
    Linear,
    SmoothStep
};

struct URHO3D_API TerrainBrushSettings
{
    float radius{4.0f};
    float strength{0.1f};
    unsigned resolution{16};
    TerrainBrushFalloff falloff{TerrainBrushFalloff::SmoothStep};

    bool Validate(std::string* error = nullptr) const;
};

/// Deterministic CPU terrain authoring primitives shared by the editor and cook pipeline.
class URHO3D_API TerrainAuthoring
{
public:
    /// Return normalized brush influence for a distance in the [0, 1] brush radius.
    static float EvaluateFalloff(TerrainBrushFalloff falloff, float normalizedDistance);
    /// Apply one height stamp to a row-major [0, 1] height field.
    static bool ApplyHeightStamp(std::vector<float>& heights, unsigned width, unsigned height,
        float centerX, float centerY, const TerrainBrushSettings& settings, std::string* error = nullptr);
    /// Quantize a normalized height to a reproducible unsigned integer.
    static unsigned QuantizeHeight(float height, unsigned bits = 16);
    /// Compute a stable digest over dimensions and quantized height samples.
    static unsigned long long ComputeDigest(const std::vector<float>& heights, unsigned width, unsigned height);
};

} // namespace Urho3D
