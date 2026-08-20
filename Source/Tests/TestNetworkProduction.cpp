// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#include <Urho3D/Blueprint/BlueprintRuntime.h>
#include <Urho3D/Replica/ReplicatedProperty.h>
#include <Urho3D/Replica/RelevancyManager.h>
#include <Urho3D/Replica/RpcDispatcher.h>
#include <Urho3D/Replica/RollbackManager.h>
#include <Urho3D/Replica/SnapshotBuffer.h>

#include <Urho3D/Math/Vector3.h>

#include <catch2/catch_amalgamated.hpp>

using namespace Urho3D;

TEST_CASE("Replicated properties capture and apply dirty deltas")
{
    ReplicatedPropertySet properties;
    CHECK(properties.Register(ReplicatedProperty("health", Variant(100), true)));
    CHECK(properties.Register(ReplicatedProperty("cosmetic", Variant(ea::string("blue")), false)));

    StringVariantMap initial = properties.CaptureDirty();
    CHECK(initial.size() == 2);
    properties.ClearDirty();
    CHECK(properties.CaptureDirty().empty());
    CHECK(properties.SetValue("health", Variant(75)));
    CHECK(properties.CaptureDirty().at("health").GetInt() == 75);
    CHECK(properties.ApplyDelta(StringVariantMap{{"health", Variant(50)}}, false));
    CHECK(properties.Find("health")->GetValue().GetInt() == 50);
    CHECK(properties.CaptureDirty().empty());
}

TEST_CASE("RPC dispatcher validates roles and invokes handlers")
{
    RpcDispatcher dispatcher;
    CHECK(dispatcher.Register({"Add", [](const StringVariantMap& arguments, StringVariantMap& outputs)
    {
        outputs["sum"] = arguments.at("a").GetInt() + arguments.at("b").GetInt();
        return true;
    }, true, false}));
    CHECK(dispatcher.Register({"ServerOnly", [](const StringVariantMap&, StringVariantMap&)
    {
        return true;
    }, true, true}));

    StringVariantMap outputs;
    CHECK(dispatcher.Dispatch("Add", StringVariantMap{{"a", Variant(2)}, {"b", Variant(3)}}, false, &outputs));
    CHECK(outputs.at("sum").GetInt() == 5);
    CHECK_FALSE(dispatcher.Dispatch("ServerOnly", {}, false));
    CHECK_FALSE(dispatcher.Dispatch("Missing", {}, true));
}

TEST_CASE("Relevancy manager filters objects by observer radius")
{
    const NetworkId nearObject = ConstructComponentReference(1, 1);
    const NetworkId farObject = ConstructComponentReference(2, 1);
    RelevancyManager relevancy;
    relevancy.SetObserver(42, Vector3::ZERO, 10.0f);
    relevancy.SetObjectRule(nearObject, Vector3(5.0f, 0.0f, 0.0f), 1.0f);
    relevancy.SetObjectRule(farObject, Vector3(100.0f, 0.0f, 0.0f), 1.0f);

    CHECK(relevancy.IsRelevant(42, nearObject));
    CHECK_FALSE(relevancy.IsRelevant(42, farObject));
    CHECK(relevancy.SetAlwaysRelevant(farObject));
    CHECK(relevancy.IsRelevant(42, farObject));

    const ea::vector<NetworkId> filtered = relevancy.Filter(42, {nearObject, farObject});
    CHECK(filtered.size() == 2);
}

TEST_CASE("Snapshot buffer interpolates numeric values and caps extrapolation")
{
    SnapshotBuffer buffer(4);
    buffer.SetMaxExtrapolation(0.25f);
    NetworkSnapshot first;
    first.frame = static_cast<NetworkFrame>(1);
    first.time = 0.0f;
    first.values["x"] = 0.0f;
    NetworkSnapshot second;
    second.frame = static_cast<NetworkFrame>(2);
    second.time = 1.0f;
    second.values["x"] = 10.0f;
    buffer.Push(first);
    buffer.Push(second);

    StringVariantMap values;
    CHECK(buffer.Sample(0.5f, values));
    CHECK_THAT(values.at("x").GetFloat(), Catch::Matchers::WithinAbs(5.0f, 0.0001f));
    CHECK(buffer.Sample(1.1f, values));
    CHECK_THAT(values.at("x").GetFloat(), Catch::Matchers::WithinAbs(11.0f, 0.0001f));
    CHECK(buffer.Sample(2.0f, values));
    CHECK_THAT(values.at("x").GetFloat(), Catch::Matchers::WithinAbs(10.0f, 0.0001f));
}

TEST_CASE("Rollback manager replays inputs after authoritative frame")
{
    RollbackManager rollback(8);
    rollback.RecordInput({static_cast<NetworkFrame>(2), StringVariantMap{{"delta", Variant(2)}}});
    rollback.RecordInput({static_cast<NetworkFrame>(3), StringVariantMap{{"delta", Variant(3)}}});

    StringVariantMap corrected;
    CHECK(rollback.Reconcile(static_cast<NetworkFrame>(1), StringVariantMap{{"value", Variant(10)}},
        [](const StringVariantMap& input, StringVariantMap& state)
        {
            state["value"] = state.at("value").GetInt() + input.at("delta").GetInt();
            return true;
        }, corrected));
    CHECK(corrected.at("value").GetInt() == 15);
    CHECK(rollback.FindState(static_cast<NetworkFrame>(3)));
}

TEST_CASE("Rollback manager orders inputs and reports prediction divergence")
{
    RollbackManager rollback(8);
    rollback.SaveState(static_cast<NetworkFrame>(2), StringVariantMap{{"value", Variant(5)}});
    rollback.RecordInput({static_cast<NetworkFrame>(4), StringVariantMap{{"delta", Variant(4)}}});
    rollback.RecordInput({static_cast<NetworkFrame>(3), StringVariantMap{{"delta", Variant(3)}}});

    StringVariantMap corrected;
    const StringVariantMap authoritative{{"value", Variant(2)}};
    CHECK(rollback.Reconcile(static_cast<NetworkFrame>(2), authoritative,
        [](const StringVariantMap& input, StringVariantMap& state)
        {
            state["value"] = state.at("value").GetInt() + input.at("delta").GetInt();
            return true;
        }, corrected));
    CHECK(corrected.at("value").GetInt() == 9);
    CHECK(rollback.GetInputs().at(0).frame == static_cast<NetworkFrame>(3));
    CHECK(rollback.GetInputs().at(1).frame == static_cast<NetworkFrame>(4));
    CHECK(rollback.GetLastDiagnostics().diverged);
    CHECK(rollback.GetLastDiagnostics().predictedInputs == 2);
    CHECK(rollback.GetLastDiagnostics().replayedInputs == 2);
    CHECK(rollback.GetLastDiagnostics().rejectedInputs == 0);
}

TEST_CASE("Rollback manager validates resynchronization checkpoints")
{
    RollbackManager rollback(8);
    const StringVariantMap state{{"tick", Variant(12)}};
    RollbackResynchronization checkpoint;
    checkpoint.frame = static_cast<NetworkFrame>(12);
    checkpoint.state = state;
    checkpoint.digest = RollbackManager::ComputeStateDigest(state);

    StringVariantMap corrected;
    CHECK(rollback.ApplyResynchronization(checkpoint,
        [](const StringVariantMap&, StringVariantMap&)
        {
            return true;
        }, corrected));
    CHECK(corrected.at("tick").GetInt() == 12);
    CHECK(rollback.GetLastDiagnostics().authoritativeDigest == checkpoint.digest);

    checkpoint.digest += 1;
    CHECK_FALSE(rollback.ApplyResynchronization(checkpoint, {}, corrected));
    CHECK(rollback.GetLastDiagnostics().diverged);
}

TEST_CASE("Rollback manager state digest is insertion-order independent")
{
    StringVariantMap first;
    first["health"] = Variant(100);
    first["round"] = Variant(3);
    first["confirmed"] = Variant(true);

    StringVariantMap second;
    second["confirmed"] = Variant(true);
    second["round"] = Variant(3);
    second["health"] = Variant(100);

    CHECK(RollbackManager::ComputeStateDigest(first) == RollbackManager::ComputeStateDigest(second));
    first["health"] = Variant(99);
    CHECK(RollbackManager::ComputeStateDigest(first) != RollbackManager::ComputeStateDigest(second));
}

TEST_CASE("Rollback manager enforces a bounded prediction window")
{
    RollbackManager rollback(8);
    rollback.SetPredictionWindow(2);

    StringVariantMap authoritative{{"tick", Variant(10)}};
    StringVariantMap corrected;
    CHECK(rollback.Reconcile(static_cast<NetworkFrame>(10), authoritative, {}, corrected));
    CHECK(rollback.GetLatestAuthoritativeFrame() == static_cast<NetworkFrame>(10));
    CHECK(rollback.GetPredictionDepth(static_cast<NetworkFrame>(12)) == 2);
    CHECK(rollback.IsPredictionAllowed(static_cast<NetworkFrame>(12)));
    CHECK_FALSE(rollback.IsPredictionAllowed(static_cast<NetworkFrame>(13)));
}

TEST_CASE("Rollback manager identifies the first divergent digest")
{
    RollbackManager rollback(8);
    rollback.SaveState(static_cast<NetworkFrame>(1), StringVariantMap{{"tick", Variant(1)}});
    rollback.SaveState(static_cast<NetworkFrame>(2), StringVariantMap{{"tick", Variant(2)}});
    rollback.SaveState(static_cast<NetworkFrame>(3), StringVariantMap{{"tick", Variant(3)}});

    ea::vector<RollbackDigestSample> authoritative;
    authoritative.push_back({static_cast<NetworkFrame>(1), rollback.FindDigest(static_cast<NetworkFrame>(1))->digest});
    authoritative.push_back({static_cast<NetworkFrame>(2), rollback.FindDigest(static_cast<NetworkFrame>(2))->digest});
    authoritative.push_back({static_cast<NetworkFrame>(3), rollback.FindDigest(static_cast<NetworkFrame>(3))->digest + 1});

    CHECK_FALSE(rollback.ValidateAuthoritativeDigests(authoritative));
    CHECK(rollback.GetLastDiagnostics().comparedDigests == 3);
    CHECK(rollback.GetLastDiagnostics().firstDivergentFrame == static_cast<NetworkFrame>(3));
    CHECK(rollback.GetLastDiagnostics().desyncDetected);
}

TEST_CASE("Blueprint runtime exposes production network nodes")
{
    BlueprintRuntime runtime;
    CHECK(runtime.GetRegistry().Find("Net.SendRPC"));
    CHECK(runtime.GetRegistry().Find("Net.SetRelevancy"));
    CHECK(runtime.GetRegistry().Find("Net.GetPing"));
    CHECK(runtime.GetRegistry().Find("Net.IsServer"));
    CHECK(runtime.GetRegistry().Find("Net.IsClient"));
}
