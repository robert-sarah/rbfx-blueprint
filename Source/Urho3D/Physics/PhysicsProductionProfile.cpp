// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#include "PhysicsProductionProfile.h"

#include <algorithm>
#include <cmath>

namespace Urho3D
{

namespace
{
constexpr float MinimumFixedStep = 1.0f / 1000.0f;
constexpr float MaximumFixedStep = 0.25f;
}

PhysicsProductionValidation PhysicsProductionProfile::Validate(const PhysicsProductionSettings& settings)
{
    PhysicsProductionValidation result;
    result.valid = true;

    if (!std::isfinite(settings.fixedTimeStep) || settings.fixedTimeStep < MinimumFixedStep
        || settings.fixedTimeStep > MaximumFixedStep)
    {
        result.errors.emplace_back("fixedTimeStep must be finite and within [0.001, 0.25] seconds");
    }
    if (settings.maxSubSteps == 0 || settings.maxSubSteps > 32)
        result.errors.emplace_back("maxSubSteps must be within [1, 32]");
    if (settings.solverIterations == 0 || settings.solverIterations > 128)
        result.errors.emplace_back("solverIterations must be within [1, 128]");
    if (settings.maximumBodies == 0)
        result.errors.emplace_back("maximumBodies must be greater than zero");
    if (settings.maximumContacts == 0)
        result.errors.emplace_back("maximumContacts must be greater than zero");

    if (settings.enabledFeatures.find(PhysicsProductionFeature::Fluid) != settings.enabledFeatures.end()
        && settings.enabledFeatures.at(PhysicsProductionFeature::Fluid) && settings.maximumContacts < 1024)
    {
        result.warnings.emplace_back("fluid simulations usually require at least 1024 contact slots");
    }
    if (settings.enabledFeatures.find(PhysicsProductionFeature::Cloth) != settings.enabledFeatures.end()
        && settings.enabledFeatures.at(PhysicsProductionFeature::Cloth) && settings.solverIterations < 4)
    {
        result.warnings.emplace_back("cloth is enabled with fewer than four solver iterations");
    }

    result.valid = result.errors.empty();
    return result;
}

std::vector<PhysicsProductionFeature> PhysicsProductionProfile::EnabledFeatures(
    const PhysicsProductionSettings& settings)
{
    std::vector<PhysicsProductionFeature> result;
    for (const auto& entry : settings.enabledFeatures)
    {
        if (entry.second)
            result.push_back(entry.first);
    }
    return result;
}

const char* PhysicsProductionProfile::FeatureName(PhysicsProductionFeature feature)
{
    switch (feature)
    {
    case PhysicsProductionFeature::Destruction: return "destruction";
    case PhysicsProductionFeature::Cloth: return "cloth";
    case PhysicsProductionFeature::SoftBody: return "soft_body";
    case PhysicsProductionFeature::Fluid: return "fluid";
    case PhysicsProductionFeature::Vehicle: return "vehicle";
    case PhysicsProductionFeature::Ragdoll: return "ragdoll";
    default: return "unknown";
    }
}

} // namespace Urho3D

