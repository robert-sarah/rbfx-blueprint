// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#include <Urho3D/AI/ProductionBehaviorTree.h>
#include <Urho3D/Network/ProductionOnlineSession.h>
#include <Urho3D/UI/ProductionAccessibilityProfile.h>

#include <catch2/catch_amalgamated.hpp>

using namespace Urho3D;

TEST_CASE("ProductionBehaviorTree evaluates sequence and selector nodes deterministically", "[ai][behavior-tree]")
{
    ProductionBehaviorTree tree;
    REQUIRE(tree.AddNode({"Root", "", ProductionBehaviorNodeType::Sequence, ""}));
    REQUIRE(tree.AddNode({"HasTarget", "Root", ProductionBehaviorNodeType::Condition, "has_target"}));
    REQUIRE(tree.AddNode({"Move", "Root", ProductionBehaviorNodeType::Action, "move_to_target"}));
    REQUIRE(tree.SetCondition("has_target", true));
    REQUIRE(tree.SetActionStatus("Move", ProductionBehaviorStatus::Success));
    CHECK(tree.Tick() == ProductionBehaviorStatus::Success);

    REQUIRE(tree.SetActionStatus("Move", ProductionBehaviorStatus::Running));
    CHECK(tree.Tick() == ProductionBehaviorStatus::Running);
    REQUIRE(tree.SetCondition("has_target", false));
    CHECK(tree.Tick() == ProductionBehaviorStatus::Failure);

    CHECK_FALSE(tree.SetActionStatus("HasTarget", ProductionBehaviorStatus::Success));
    CHECK(tree.GetNodeCount() == 3);
}

TEST_CASE("ProductionOnlineSession enforces handshake, capacity and rollback policy", "[network][session]")
{
    ProductionOnlineSessionSettings settings;
    settings.sessionId = "ranked-1v1";
    settings.role = ProductionSessionRole::DedicatedServer;
    settings.maxPlayers = 2;
    settings.tickRate = 60;
    settings.inputDelayFrames = 3;
    settings.rollbackWindowFrames = 120;

    ProductionOnlineSession session;
    REQUIRE(session.Configure(settings));
    CHECK_FALSE(session.AddPeer("player-a", false));
    REQUIRE(session.AddPeer("player-a", true));
    REQUIRE(session.AddPeer("player-b", true));
    CHECK_FALSE(session.AddPeer("spectator", true));
    CHECK_FALSE(session.AddPeer("player-a", true));
    CHECK(session.HasPeer("player-a"));
    CHECK(session.GetPeerCount() == 2);
    REQUIRE(session.RemovePeer("player-b"));
    CHECK(session.GetPeerCount() == 1);

    settings.inputDelayFrames = 121;
    const ProductionOnlineSessionValidation invalid = ProductionOnlineSession::Validate(settings);
    CHECK_FALSE(invalid.valid);
    CHECK_FALSE(invalid.errors.empty());
}

TEST_CASE("ProductionAccessibilityProfile validates and scales runtime UI policies", "[ui][accessibility]")
{
    ProductionAccessibilitySettings settings;
    settings.textScale = 1.5f;
    settings.interfaceScale = 1.25f;
    settings.highContrast = true;
    settings.reducedMotion = true;
    settings.subtitles = true;

    const ProductionAccessibilityValidation validation = ProductionAccessibilityProfile::Validate(settings);
    REQUIRE(validation.valid);
    CHECK(ProductionAccessibilityProfile::ScaleText(16.0f, settings) == Catch::Approx(24.0f));
    CHECK(ProductionAccessibilityProfile::ScaleMotionDuration(0.5f, settings) == Catch::Approx(0.0f));
    CHECK(ProductionAccessibilityProfile::RequiresSubtitles(settings));

    settings.textScale = 4.0f;
    CHECK_FALSE(ProductionAccessibilityProfile::Validate(settings).valid);
}
