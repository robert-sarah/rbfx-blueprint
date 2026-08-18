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
