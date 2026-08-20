// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include "../Core/StringUtils.h"
#include "../Math/Vector3.h"

namespace Urho3D
{

/// Input to the backend-neutral spatial audio evaluator.
struct URHO3D_API AudioSpatialSource
{
    Vector3 sourcePosition{Vector3::ZERO};
    float sourceVolume{1.0f};
    float minDistance{1.0f};
    float maxDistance{100.0f};
    bool enabled{true};
};

/// Listener basis used for deterministic left/right panning.
struct URHO3D_API AudioSpatialListener
{
    Vector3 position{Vector3::ZERO};
    Vector3 right{Vector3::RIGHT};
};

/// Result consumed by a platform audio backend or mixer bus.
struct URHO3D_API AudioSpatialResult
{
    float attenuation{1.0f};
    float pan{};
    float gain{1.0f};
    float leftGain{};
    float rightGain{};
    bool audible{true};
};

/// Deterministic 3D spatialization contract for stereo or HRTF backends.
///
/// This class intentionally computes only the stable source-to-listener terms.
/// Device-specific HRTF convolution, reverb and occlusion probes remain backend
/// responsibilities and can consume the returned gain and pan values.
class URHO3D_API AudioSpatializer
{
public:
    static AudioSpatialResult Evaluate(const AudioSpatialSource& source, const AudioSpatialListener& listener);

private:
    static float Clamp01(float value);
    static float ClampSigned(float value);
};

} // namespace Urho3D

