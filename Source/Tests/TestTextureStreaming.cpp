// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#include <catch2/catch_amalgamated.hpp>

#include <Urho3D/Graphics/TextureStreaming.h>

using namespace Urho3D;

TEST_CASE("Texture streaming planner prioritizes visible textures deterministically", "[render][streaming]")
{
    TextureStreamingManager manager;
    REQUIRE(manager.RegisterTexture("hero", {80, 40, 20}, 2));
    REQUIRE(manager.RegisterTexture("background", {120, 60, 30}, 0));
    manager.SetBudgetBytes(170);

    const std::vector<TextureStreamingRequest> requests{
        {"background", 0, 1.0f},
        {"hero", 0, 10.0f},
    };
    const std::vector<TextureStreamingDecision> decisions = manager.BuildPlan(requests);

    REQUIRE(decisions.size() == 2);
    REQUIRE(decisions[0].resourceName == "hero");
    REQUIRE(decisions[0].targetMip == 0);
    REQUIRE(decisions[0].load);
    REQUIRE(decisions[1].resourceName == "background");
    REQUIRE(decisions[1].targetMip == 2);
    REQUIRE_FALSE(decisions[1].load);
    REQUIRE(decisions[1].evict);
    REQUIRE(decisions[0].bytesRequired + decisions[1].bytesRequired <= manager.GetBudgetBytes());
}

TEST_CASE("Texture streaming planner uses name as stable priority tie breaker", "[render][streaming][determinism]")
{
    TextureStreamingManager manager;
    REQUIRE(manager.RegisterTexture("zeta", {30, 10}, 1));
    REQUIRE(manager.RegisterTexture("alpha", {30, 10}, 1));
    manager.SetBudgetBytes(40);

    const std::vector<TextureStreamingRequest> requests{
        {"zeta", 0, 5.0f},
        {"alpha", 0, 5.0f},
    };
    const std::vector<TextureStreamingDecision> decisions = manager.BuildPlan(requests);
    REQUIRE(decisions.size() == 2);
    REQUIRE(decisions[0].resourceName == "alpha");
    REQUIRE(decisions[0].targetMip == 0);
    REQUIRE(decisions[1].resourceName == "zeta");
    REQUIRE(decisions[1].targetMip == 1);
}

TEST_CASE("Texture streaming planner rejects invalid registrations", "[render][streaming][validation]")
{
    TextureStreamingManager manager;
    REQUIRE_FALSE(manager.RegisterTexture({}, {10, 5}));
    REQUIRE_FALSE(manager.RegisterTexture("empty", {}));
    REQUIRE_FALSE(manager.RegisterTexture("zero", {10, 0}));
    REQUIRE(manager.RegisterTexture("valid", {10, 5}));
    REQUIRE_FALSE(manager.RegisterTexture("valid", {10, 5}) == false);
    REQUIRE(manager.UnregisterTexture("valid"));
    REQUIRE_FALSE(manager.UnregisterTexture("valid"));
}
