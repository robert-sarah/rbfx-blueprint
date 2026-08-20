// SPDX-License-Identifier: MIT
#pragma once

#include "../Core/StringUtils.h"
#include "../Resource/JSONValue.h"

#include <string>
#include <vector>

namespace Urho3D
{

enum class ModelUpAxis : unsigned
{
    X,
    Y,
    Z
};

enum class ModelHandedness : unsigned
{
    Left,
    Right
};

/// Deterministic import contract for mesh and scene model sources.
struct URHO3D_API ModelImportProfile
{
    unsigned version{1};
    std::string sourceFormat{"gltf"};
    ModelUpAxis upAxis{ModelUpAxis::Y};
    ModelHandedness handedness{ModelHandedness::Right};
    double unitsPerMeter{1.0};
    bool generateTangents{true};
    bool generateLightmapUV{false};
    unsigned uvChannelCount{1};
    std::vector<float> lodScreenSizes{0.75f, 0.35f, 0.15f};
    std::string provenance{"unspecified"};

    /// Validate the profile without touching filesystem or importer state.
    bool Validate(std::string* error = nullptr) const;
    /// Serialize the profile in a stable JSON schema.
    JSONValue ToJSON() const;
    /// Restore and validate a profile from its complete JSON schema.
    bool FromJSON(const JSONValue& value, std::string* error = nullptr);
    /// Return a stable hash over the normalized profile values.
    unsigned CalculateHash() const;
};

} // namespace Urho3D
