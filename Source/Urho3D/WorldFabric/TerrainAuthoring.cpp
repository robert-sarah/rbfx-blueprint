// SPDX-License-Identifier: MIT

#include "TerrainAuthoring.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Urho3D
{

namespace
{

void SetError(std::string* error, const char* message)
{
    if (error)
        *error = message;
}

void HashByte(unsigned long long& hash, unsigned char value)
{
    hash ^= value;
    hash *= 1099511628211ull;
}

void HashUnsigned(unsigned long long& hash, unsigned value)
{
    for (unsigned shift = 0; shift < 4; ++shift)
        HashByte(hash, static_cast<unsigned char>((value >> (shift * 8)) & 0xff));
}

} // namespace

bool TerrainBrushSettings::Validate(std::string* error) const
{
    if (!std::isfinite(radius) || radius <= 0.0f)
    {
        SetError(error, "Terrain brush radius must be finite and positive.");
        return false;
    }
    if (!std::isfinite(strength) || strength < -1.0f || strength > 1.0f)
    {
        SetError(error, "Terrain brush strength must be finite and within [-1, 1].");
        return false;
    }
    if (resolution < 2 || resolution > 4096)
    {
        SetError(error, "Terrain brush resolution must be in the [2, 4096] range.");
        return false;
    }
    return true;
}

float TerrainAuthoring::EvaluateFalloff(TerrainBrushFalloff falloff, float normalizedDistance)
{
    const float distance = std::clamp(normalizedDistance, 0.0f, 1.0f);
    switch (falloff)
    {
    case TerrainBrushFalloff::Constant:
        return 1.0f;
    case TerrainBrushFalloff::Linear:
        return 1.0f - distance;
    case TerrainBrushFalloff::SmoothStep:
        return 1.0f - distance * distance * (3.0f - 2.0f * distance);
    default:
        return 0.0f;
    }
}

bool TerrainAuthoring::ApplyHeightStamp(std::vector<float>& heights, unsigned width, unsigned height,
    float centerX, float centerY, const TerrainBrushSettings& settings, std::string* error)
{
    if (!settings.Validate(error))
        return false;
    if (width < 2 || height < 2 || heights.size() != static_cast<size_t>(width) * height)
    {
        SetError(error, "Terrain height field dimensions do not match the sample buffer.");
        return false;
    }
    if (!std::isfinite(centerX) || !std::isfinite(centerY))
    {
        SetError(error, "Terrain brush center must be finite.");
        return false;
    }

    for (unsigned y = 0; y < height; ++y)
    {
        for (unsigned x = 0; x < width; ++x)
        {
            const float dx = static_cast<float>(x) - centerX;
            const float dy = static_cast<float>(y) - centerY;
            const float distance = std::sqrt(dx * dx + dy * dy);
            if (distance > settings.radius)
                continue;

            const float influence = EvaluateFalloff(settings.falloff, distance / settings.radius);
            const size_t index = static_cast<size_t>(y) * width + x;
            const float source = std::isfinite(heights[index]) ? heights[index] : 0.0f;
            heights[index] = std::clamp(source + settings.strength * influence, 0.0f, 1.0f);
        }
    }
    return true;
}

unsigned TerrainAuthoring::QuantizeHeight(float height, unsigned bits)
{
    if (bits == 0 || bits > 24)
        bits = 16;
    const unsigned maximum = (1u << bits) - 1u;
    const float normalized = std::clamp(std::isfinite(height) ? height : 0.0f, 0.0f, 1.0f);
    return static_cast<unsigned>(std::lround(normalized * static_cast<float>(maximum)));
}

unsigned long long TerrainAuthoring::ComputeDigest(const std::vector<float>& heights, unsigned width, unsigned height)
{
    unsigned long long hash = 1469598103934665603ull;
    HashUnsigned(hash, width);
    HashUnsigned(hash, height);
    const size_t expected = static_cast<size_t>(width) * height;
    for (size_t index = 0; index < expected; ++index)
        HashUnsigned(hash, index < heights.size() ? QuantizeHeight(heights[index]) : 0u);
    return hash;
}

} // namespace Urho3D
