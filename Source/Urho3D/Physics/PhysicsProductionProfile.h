// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include "../Core/StringUtils.h"

#include <map>
#include <string>
#include <vector>

namespace Urho3D
{

enum class PhysicsProductionFeature
{
    Destruction,
    Cloth,
    SoftBody,
    Fluid,
    Vehicle,
    Ragdoll
};

/// Deterministic authoring and runtime limits shared by 2D/3D physics profiles.
struct URHO3D_API PhysicsProductionSettings
{
    float fixedTimeStep{1.0f / 60.0f};
    unsigned maxSubSteps{4};
    unsigned solverIterations{8};
    unsigned maximumBodies{4096};
    unsigned maximumContacts{16384};
    std::map<PhysicsProductionFeature, bool> enabledFeatures;
};

/// Validation result for a production physics configuration.
struct URHO3D_API PhysicsProductionValidation
{
    bool valid{};
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

/// Backend-neutral validator for deterministic physics authoring contracts.
///
/// The validator does not pretend to implement a cloth or fluid solver. It
/// ensures that a project declares safe fixed-step limits and that optional
/// backend features are explicit before runtime initialization.
class URHO3D_API PhysicsProductionProfile
{
public:
    static PhysicsProductionValidation Validate(const PhysicsProductionSettings& settings);
    static std::vector<PhysicsProductionFeature> EnabledFeatures(const PhysicsProductionSettings& settings);
    static const char* FeatureName(PhysicsProductionFeature feature);
};

} // namespace Urho3D

