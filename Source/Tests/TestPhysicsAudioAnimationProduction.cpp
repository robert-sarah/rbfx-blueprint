// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#include <Urho3D/Animation/ProductionAnimationLayerStack.h>
#include <Urho3D/Audio/AudioSpatializer.h>
#include <Urho3D/Physics/PhysicsProductionProfile.h>

#include <catch2/catch_amalgamated.hpp>

using namespace Urho3D;

TEST_CASE("AudioSpatializer evaluates deterministic stereo gains", "[audio][spatial]")
{
    AudioSpatialSource source;
    source.sourcePosition = Vector3(5.0f, 0.0f, 0.0f);
    source.sourceVolume = 0.8f;
    source.minDistance = 1.0f;
    source.maxDistance = 10.0f;

    const AudioSpatialResult result = AudioSpatializer::Evaluate(source, {});
    CHECK(result.audible);
    CHECK(result.pan == Catch::Approx(1.0f));
    CHECK(result.attenuation > 0.0f);
    CHECK(result.leftGain == Catch::Approx(0.0f));
    CHECK(result.rightGain == Catch::Approx(result.gain));

    source.sourcePosition = Vector3(100.0f, 0.0f, 0.0f);
    const AudioSpatialResult inaudible = AudioSpatializer::Evaluate(source, {});
    CHECK_FALSE(inaudible.audible);
    CHECK(inaudible.leftGain == Catch::Approx(0.0f));
    CHECK(inaudible.rightGain == Catch::Approx(0.0f));
}

TEST_CASE("PhysicsProductionProfile validates fixed-step budgets and features", "[physics][production]")
{
    PhysicsProductionSettings settings;
    settings.maximumBodies = 2048;
    settings.maximumContacts = 4096;
    settings.enabledFeatures[PhysicsProductionFeature::Vehicle] = true;
    settings.enabledFeatures[PhysicsProductionFeature::Ragdoll] = true;

    const PhysicsProductionValidation valid = PhysicsProductionProfile::Validate(settings);
    REQUIRE(valid.valid);
    CHECK(valid.errors.empty());
    CHECK(PhysicsProductionProfile::FeatureName(PhysicsProductionFeature::Ragdoll) == std::string("ragdoll"));
    CHECK(PhysicsProductionProfile::EnabledFeatures(settings).size() == 2);

    settings.fixedTimeStep = 1.0f;
    const PhysicsProductionValidation invalid = PhysicsProductionProfile::Validate(settings);
    CHECK_FALSE(invalid.valid);
    CHECK_FALSE(invalid.errors.empty());
}

TEST_CASE("ProductionAnimationLayerStack normalizes base layers and preserves additive layers", "[animation][production]")
{
    ProductionAnimationLayerStack stack;
    REQUIRE(stack.SetLayer({"Base", "Run", 0.75f, false, true}));
    REQUIRE(stack.SetLayer({"UpperBody", "Aim", 0.25f, false, true}));
    REQUIRE(stack.SetLayer({"Recoil", "Recoil", 0.4f, true, true}));
    REQUIRE(stack.SetLayer({"Disabled", "Unused", 1.0f, false, false}));

    const std::vector<ProductionAnimationContribution> contributions = stack.Evaluate();
    REQUIRE(contributions.size() == 3);
    CHECK(contributions[0].layer == "Base");
    CHECK(contributions[0].weight == Catch::Approx(0.75f));
    CHECK(contributions[1].layer == "UpperBody");
    CHECK(contributions[1].weight == Catch::Approx(0.25f));
    CHECK(contributions[2].layer == "Recoil");
    CHECK(contributions[2].weight == Catch::Approx(0.4f));
    CHECK(contributions[2].additive);

    CHECK_FALSE(stack.SetLayer({"", "Invalid", 1.0f, false, true}));
    CHECK_FALSE(stack.SetLayer({"InvalidWeight", "Invalid", -1.0f, false, true}));
}
