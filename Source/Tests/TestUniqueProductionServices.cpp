// Copyright (c) 2026 the rbfx-blueprint project.
//
// SPDX-License-Identifier: MIT
//

#include <Urho3D/WorldFabric/CausalWorldFabricDebugger.h>
#include <Urho3D/WorldFabric/SemanticBuildCapsule.h>
#include <Urho3D/WorldFabric/UniversalDeterministicTimeMachine.h>

#include <catch2/catch_amalgamated.hpp>

using namespace Urho3D;

TEST_CASE("Causal World Fabric debugger records chains and impacted nodes", "[worldfabric][causal]")
{
    WorldFabricGraph graph;
    CausalWorldFabricDebugger debugger(&graph);
    const WorldFabricId asset = graph.AddNode("asset/player", WorldFabricNodeKind::Asset, "Texture");
    const WorldFabricId blueprint = graph.AddNode("blueprint/player", WorldFabricNodeKind::Blueprint, "Blueprint");
    REQUIRE(asset != InvalidWorldFabricId);
    REQUIRE(blueprint != InvalidWorldFabricId);
    REQUIRE(graph.AddDependency(blueprint, asset, WorldFabricDependencyKind::BuildsFrom));

    const unsigned long long build = debugger.RecordBuild(asset, "asset-import", "Imported player asset", 11, 2);
    REQUIRE(build != 0);
    const unsigned long long runtime = debugger.RecordRuntime(blueprint, "blueprint-runtime", "Blueprint evaluation failed",
        4, 22, build, 0.9);
    REQUIRE(runtime != 0);

    const CausalAnalysis analysis = debugger.Analyze(blueprint);
    REQUIRE(analysis.found);
    REQUIRE(analysis.chain.size() >= 2);
    CHECK(analysis.chain.front().node == asset);
    CHECK(analysis.chain.back().node == blueprint);
    CHECK(std::find(analysis.impactedNodes.begin(), analysis.impactedNodes.end(), blueprint) != analysis.impactedNodes.end());
    CHECK(debugger.ComputeDigest() != 0);
}

TEST_CASE("Universal deterministic time machine branches and finds divergence", "[worldfabric][determinism]")
{
    UniversalDeterministicTimeMachine machine(16);
    StringVariantMap initial;
    initial["score"] = 0;
    REQUIRE(machine.Start(initial));

    const UniversalDeterministicStep step = [](unsigned, float, const StringVariantMap& input,
        const StringVariantMap& current, StringVariantMap& next)
    {
        next = current;
        next["score"] = current.find("score")->second.GetInt() + input.find("delta")->second.GetInt();
        return true;
    };

    StringVariantMap input;
    input["delta"] = 1;
    REQUIRE(machine.Advance(input, step));
    CHECK(machine.GetState().find("score")->second.GetInt() == 1);
    REQUIRE(machine.CreateBranch("alternative", 1));

    input["delta"] = 5;
    REQUIRE(machine.Advance(input, step, DeterministicTimeMachineDomain::Gameplay, "alternative step"));
    CHECK(machine.GetState().find("score")->second.GetInt() == 6);

    unsigned divergenceFrame = 0;
    DeterministicFrameDifference difference;
    REQUIRE(machine.FindFirstDivergence("main", "alternative", divergenceFrame, difference));
    CHECK(divergenceFrame == 2);
    CHECK(difference.valid);
    CHECK(difference.fromDigest != difference.toDigest);
    CHECK(std::find(difference.changedKeys.begin(), difference.changedKeys.end(), "score") != difference.changedKeys.end());

    REQUIRE(machine.SwitchBranch("main"));
    REQUIRE(machine.Restore(1));
    input["delta"] = 2;
    REQUIRE(machine.Advance(input, step));
    CHECK(machine.GetState().find("score")->second.GetInt() == 3);
    CHECK(machine.ComputeDigest() != 0);
}

TEST_CASE("Universal deterministic time machine exports and imports branch replays", "[worldfabric][determinism][replay]")
{
    UniversalDeterministicTimeMachine machine(16);
    StringVariantMap initial;
    initial["score"] = Variant(0);
    REQUIRE(machine.Start(initial));

    const UniversalDeterministicStep step = [](unsigned, float, const StringVariantMap& input,
        const StringVariantMap& current, StringVariantMap& next)
    {
        next = current;
        next["score"] = current.at("score").GetInt() + input.at("delta").GetInt();
        return true;
    };

    StringVariantMap input;
    input["delta"] = Variant(1);
    REQUIRE(machine.Advance(input, step));
    REQUIRE(machine.CreateBranch("alternative", 1));
    input["delta"] = Variant(5);
    REQUIRE(machine.Advance(input, step, DeterministicTimeMachineDomain::Gameplay, "alternative step"));

    const std::string replay = machine.ExportReplay();
    REQUIRE_FALSE(replay.empty());

    UniversalDeterministicTimeMachine restored;
    std::string error;
    REQUIRE(restored.ImportReplay(replay, &error));
    CHECK(error.empty());
    CHECK(restored.GetCurrentBranch() == "alternative");
    CHECK(restored.GetCurrentFrame() == 2);
    CHECK(restored.GetBranches() == machine.GetBranches());
    CHECK(restored.ComputeDigest() == machine.ComputeDigest());
    CHECK(restored.ExportReplay() == replay);

    unsigned divergenceFrame = 0;
    DeterministicFrameDifference difference;
    REQUIRE(restored.FindFirstDivergence("main", "alternative", divergenceFrame, difference));
    CHECK(divergenceFrame == 2);
    CHECK(difference.valid);
    CHECK(difference.fromDigest != difference.toDigest);
}

TEST_CASE("Universal deterministic time machine rejects tampered replay atomically", "[worldfabric][determinism][replay]")
{
    UniversalDeterministicTimeMachine machine(8);
    StringVariantMap initial;
    initial["value"] = Variant(10);
    REQUIRE(machine.Start(initial));

    const UniversalDeterministicStep step = [](unsigned, float, const StringVariantMap& input,
        const StringVariantMap& current, StringVariantMap& next)
    {
        next = current;
        next["value"] = current.at("value").GetInt() + input.at("delta").GetInt();
        return true;
    };

    StringVariantMap input;
    input["delta"] = Variant(2);
    REQUIRE(machine.Advance(input, step));
    const unsigned originalFrame = machine.GetCurrentFrame();
    const unsigned long long originalDigest = machine.ComputeDigest();

    JSONValue tampered = machine.ToJSON();
    tampered["branches"][0]["frames"][0].Set("digest", "1");

    UniversalDeterministicTimeMachine restored;
    std::string error;
    CHECK_FALSE(restored.FromJSON(tampered, &error));
    CHECK_FALSE(error.empty());

    REQUIRE(restored.Start(initial));
    CHECK(restored.GetCurrentFrame() == 0);
    CHECK(restored.ComputeDigest() != originalDigest);
    CHECK(machine.GetCurrentFrame() == originalFrame);
}

TEST_CASE("Semantic build capsule validates serializes and diffs production evidence", "[worldfabric][capsule]")
{
    SemanticBuildCapsule capsule;
    SemanticBuildCapsuleMetadata metadata;
    metadata.engineRevision = "rbfx-blueprint@9de13fa";
    metadata.toolchain = "gcc-13-cxx17";
    metadata.platform = "Linux";
    metadata.architecture = "x86_64";
    metadata.configuration = "Debug";
    metadata.worldFabricDigest = 101;
    metadata.timeMachineDigest = 202;
    capsule.SetMetadata(metadata);
    REQUIRE(capsule.AddEntry({"Scenes/Player.scene", "scene", "Linux", 4096, 303}));
    REQUIRE(capsule.AddEntry({"Scripts/player.rbscript", "rbscript", "Any", 1024, 404}));
    REQUIRE(capsule.AddPlugin({"rbfx.blueprint", "1.0.0", 505}));
    REQUIRE(capsule.Validate());

    const JSONValue json = capsule.ToJSON();
    SemanticBuildCapsule restored;
    std::string error;
    REQUIRE(restored.FromJSON(json, &error));
    CHECK(restored.ComputeDigest() == capsule.ComputeDigest());
    CHECK(restored.ToCanonicalText() == capsule.ToCanonicalText());

    SemanticBuildCapsule changed = restored;
    changed.Clear();
    changed.SetMetadata(metadata);
    REQUIRE(changed.AddEntry({"Scenes/Player.scene", "scene", "Linux", 4096, 999}));
    REQUIRE(changed.AddPlugin({"rbfx.blueprint", "1.1.0", 506}));
    const SemanticCapsuleDiff diff = capsule.Diff(changed);
    REQUIRE(diff.valid);
    CHECK(diff.metadataChanged == false);
    CHECK(diff.changedEntries == std::vector<std::string>{"Scenes/Player.scene"});
    CHECK(diff.changedPlugins == std::vector<std::string>{"rbfx.blueprint"});
    CHECK(capsule.ComputeDigest() != changed.ComputeDigest());
}


TEST_CASE("Unique production services reject invalid inputs deterministically", "[worldfabric][robustness]")
{
    UniversalDeterministicTimeMachine machine(4);
    StringVariantMap initial;
    initial["value"] = Variant(0);
    REQUIRE(machine.Start(initial));

    std::string error;
    CHECK_FALSE(machine.CreateBranch("", 0, &error));
    CHECK_FALSE(machine.CreateBranch("missing-checkpoint", 4, &error));
    CHECK_FALSE(machine.SwitchBranch("missing", &error));
    CHECK_FALSE(machine.Restore(99));
    CHECK_FALSE(machine.ReplayTo(1, {}));

    SemanticBuildCapsule invalid;
    CHECK_FALSE(invalid.Validate(&error));

    SemanticBuildCapsuleMetadata metadata;
    metadata.engineRevision = "engine";
    metadata.toolchain = "toolchain";
    metadata.platform = "Linux";
    metadata.architecture = "x86_64";
    metadata.configuration = "Debug";
    metadata.worldFabricDigest = 1;
    invalid.SetMetadata(metadata);
    CHECK(invalid.AddEntry({"asset", "asset", "Any", 1, 2}));
    CHECK_FALSE(invalid.AddEntry({"asset", "asset", "Any", 1, 3}, &error));

    JSONValue malformed(JSON_OBJECT);
    SemanticBuildCapsule restored;
    CHECK_FALSE(restored.FromJSON(malformed, &error));
}

TEST_CASE("Semantic build capsule canonicalization preserves delimiter-rich fields", "[worldfabric][capsule][robustness]")
{
    SemanticBuildCapsule capsule;
    SemanticBuildCapsuleMetadata metadata;
    metadata.engineRevision = "engine|revision\n2026";
    metadata.toolchain = "tool:chain";
    metadata.platform = "Linux";
    metadata.architecture = "x86_64";
    metadata.configuration = "Debug";
    metadata.worldFabricDigest = 10;
    metadata.timeMachineDigest = 20;
    capsule.SetMetadata(metadata);
    REQUIRE(capsule.AddEntry({"Assets/player|variant\n.scene", "scene|source", "Any", 1, 2}));
    REQUIRE(capsule.AddPlugin({"plugin|sample", "1.0.0|dev", 3}));
    REQUIRE(capsule.Validate());

    const std::string canonical = capsule.ToCanonicalText();
    const JSONValue json = capsule.ToJSON();
    SemanticBuildCapsule restored;
    REQUIRE(restored.FromJSON(json));
    CHECK(restored.ToCanonicalText() == canonical);
    CHECK(restored.ComputeDigest() == capsule.ComputeDigest());
}

TEST_CASE("Causal debugger records graph changes and remains safe across detach", "[worldfabric][causal][robustness]")
{
    WorldFabricGraph graph;
    CausalWorldFabricDebugger debugger;
    REQUIRE(debugger.Attach(&graph));
    const WorldFabricId first = graph.AddNode("first", WorldFabricNodeKind::Asset, "Asset");
    REQUIRE(first != InvalidWorldFabricId);
    CHECK_FALSE(debugger.GetEvidence().empty());
    debugger.Detach();
    const size_t evidenceCount = debugger.GetEvidence().size();
    graph.AddNode("second", WorldFabricNodeKind::Asset, "Asset");
    CHECK(debugger.GetEvidence().size() == evidenceCount);
}
