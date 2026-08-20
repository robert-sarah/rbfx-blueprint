// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#include "AudioSpatializer.h"

#include <algorithm>
#include <cmath>

namespace Urho3D
{

float AudioSpatializer::Clamp01(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

float AudioSpatializer::ClampSigned(float value)
{
    return std::clamp(value, -1.0f, 1.0f);
}

AudioSpatialResult AudioSpatializer::Evaluate(const AudioSpatialSource& source, const AudioSpatialListener& listener)
{
    AudioSpatialResult result;
    if (!source.enabled)
    {
        result.gain = 0.0f;
        result.leftGain = 0.0f;
        result.rightGain = 0.0f;
        result.audible = false;
        return result;
    }

    const float minimumDistance = std::max(source.minDistance, 0.001f);
    const float maximumDistance = std::max(source.maxDistance, minimumDistance);
    const float distance = (source.sourcePosition - listener.position).Length();

    if (distance <= minimumDistance)
        result.attenuation = 1.0f;
    else if (distance >= maximumDistance)
        result.attenuation = 0.0f;
    else
    {
        const float normalizedDistance = (distance - minimumDistance) / (maximumDistance - minimumDistance);
        const float smoothDistance = normalizedDistance * normalizedDistance * (3.0f - 2.0f * normalizedDistance);
        result.attenuation = 1.0f - smoothDistance;
    }

    Vector3 right = listener.right;
    if (right.Length() <= 0.0001f)
        right = Vector3::RIGHT;
    else
        right.Normalize();

    const Vector3 offset = source.sourcePosition - listener.position;
    if (offset.Length() > 0.0001f)
    {
        const Vector3 direction = offset / offset.Length();
        result.pan = ClampSigned(direction.x_ * right.x_ + direction.y_ * right.y_ + direction.z_ * right.z_);
    }

    result.gain = std::max(source.sourceVolume, 0.0f) * result.attenuation;
    const float leftPan = std::sqrt(Clamp01((1.0f - result.pan) * 0.5f));
    const float rightPan = std::sqrt(Clamp01((1.0f + result.pan) * 0.5f));
    result.leftGain = result.gain * leftPan;
    result.rightGain = result.gain * rightPan;
    result.audible = result.gain > 0.0001f;
    return result;
}

} // namespace Urho3D

