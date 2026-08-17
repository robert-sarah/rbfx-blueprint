// SPDX-License-Identifier: MIT

#include <Urho3D/Animation/AnimationBlendSpace.h>
#include <Urho3D/Animation/AnimationMontage.h>
#include <Urho3D/Animation/AnimationRetargeter.h>
#include <Urho3D/Animation/AnimationStateMachine.h>
#include "CommonUtils.h"

#include <Urho3D/Animation/Sequencer.h>
#include <Urho3D/Animation/SequencerResource.h>
#include <Urho3D/IO/VectorBuffer.h>
#include <Urho3D/Blueprint/BlueprintRuntime.h>

#include <catch2/catch_amalgamated.hpp>

using namespace Urho3D;

TEST_CASE("Animation state machine transitions and blends", "[animation][state-machine]")
{
    AnimationStateMachine machine;
    REQUIRE(machine.AddState({"Idle", "Idle", 1.0f, 1.0f, true}));
    REQUIRE(machine.AddState({"Run", "Run", 1.0f, 1.0f, true}));

    AnimationTransition transition;
    transition.fromState = "Idle";
    transition.toState = "Run";
    transition.blendDuration = 0.2f;
    transition.minimumStateTime = 0.1f;
    transition.condition = [](const StringVariantMap& parameters)
    {
        const auto iter = parameters.find("moving");
        return iter != parameters.end() && iter->second.GetBool();
    };
    REQUIRE(machine.AddTransition(transition));
    REQUIRE(machine.Start("Idle"));
    REQUIRE(machine.SetParameter("moving", Variant(true)));
    REQUIRE(machine.Update(0.1f));
    CHECK(machine.GetCurrentState() == "Idle");
    CHECK(machine.GetNextState() == "Run");
    CHECK(machine.GetTransitionAlpha() == Catch::Approx(0.0f));
    REQUIRE(machine.Update(0.1f));
    CHECK(machine.GetTransitionAlpha() == Catch::Approx(0.5f));
    REQUIRE(machine.Update(0.1f));
    CHECK(machine.GetCurrentState() == "Run");
    CHECK(machine.GetNextState().empty());
}

TEST_CASE("Animation blend space produces normalized weights", "[animation][blend-space]")
{
    AnimationBlendSpace blendSpace(AnimationBlendSpaceDimension::OneD);
    blendSpace.SetAxisRange(Vector2(0.0f, 0.0f), Vector2(10.0f, 0.0f));
    REQUIRE(blendSpace.AddSample({"Idle", Vector2(0.0f, 0.0f)}));
    REQUIRE(blendSpace.AddSample({"Run", Vector2(10.0f, 0.0f)}));

    const ea::vector<AnimationBlendWeight> weights = blendSpace.Evaluate(2.0f);
    REQUIRE(weights.size() == 2);
    CHECK(weights[0].clip == "Idle");
    CHECK(weights[0].weight == Catch::Approx(0.8f));
    CHECK(weights[1].clip == "Run");
    CHECK(weights[1].weight == Catch::Approx(0.2f));
    CHECK(blendSpace.Evaluate(0.0f).front().weight == Catch::Approx(1.0f));
}

TEST_CASE("Animation montage emits notify tracks during playback", "[animation][montage]")
{
    AnimationMontage montage;
    montage.SetLength(2.0f);
    REQUIRE(montage.AddClip({"Intro", "Hero/Intro", 0.0f, 1.0f, 1.0f, 1.0f}));
    REQUIRE(montage.AddClip({"Loop", "Hero/Loop", 1.0f, 1.0f, 1.0f, 1.0f}));
    REQUIRE(montage.AddNotify({"Footstep", 0.5f, Variant(3)}));
    REQUIRE(montage.Play());
    REQUIRE(montage.Update(0.6f));

    const ea::vector<AnimationNotifyEvent> events = montage.ConsumeNotifies();
    REQUIRE(events.size() == 1);
    CHECK(events.front().name == "Footstep");
    CHECK(events.front().payload.GetInt() == 3);
    REQUIRE(montage.Seek(1.2f));
    REQUIRE(montage.GetActiveClip() != nullptr);
    CHECK(montage.GetActiveClip()->name == "Loop");
}

TEST_CASE("Animation retargeter auto maps and scales root motion", "[animation][retarget]")
{
    AnimationRetargeter retargeter;
    const ea::vector<AnimationRetargetBone> sourceBones{{"Root", -1, Vector3::ZERO, Quaternion::IDENTITY, Vector3::ONE},
        {"Hand", 0, Vector3(1.0f, 0.0f, 0.0f), Quaternion::IDENTITY, Vector3::ONE}};
    const ea::vector<AnimationRetargetBone> targetBones{{"root", -1, Vector3::ZERO, Quaternion::IDENTITY, Vector3::ONE},
        {"hand", 0, Vector3(2.0f, 0.0f, 0.0f), Quaternion::IDENTITY, Vector3::ONE}};
    REQUIRE(retargeter.SetSourceSkeleton(sourceBones));
    REQUIRE(retargeter.SetTargetSkeleton(targetBones));
    CHECK(retargeter.AutoMapByName(true) == 2);
    retargeter.SetRootScale(2.0f);

    const ea::vector<AnimationRetargetTransform> sourcePose{{Vector3(2.0f, 0.0f, 0.0f), Quaternion::IDENTITY, Vector3::ONE},
        {Vector3(1.5f, 0.0f, 0.0f), Quaternion::IDENTITY, Vector3::ONE}};
    ea::vector<AnimationRetargetTransform> targetPose;
    REQUIRE(retargeter.RetargetPose(sourcePose, targetPose));
    REQUIRE(targetPose.size() == 2);
    CHECK(targetPose[0].position.x_ == Catch::Approx(4.0f));
    CHECK(targetPose[1].position.x_ == Catch::Approx(2.5f));
}

TEST_CASE("Sequencer interpolates tracks and emits cinematic events", "[animation][sequencer]")
{
    Sequencer sequencer;
    sequencer.SetDuration(2.0f);
    REQUIRE(sequencer.AddTrack("Camera", SequencerTrackType::Transform));
    REQUIRE(sequencer.AddTrack("Events", SequencerTrackType::Event));
    REQUIRE(sequencer.AddKeyframe("Camera", {0.0f, Variant(0.0f)}));
    REQUIRE(sequencer.AddKeyframe("Camera", {2.0f, Variant(10.0f)}));
    REQUIRE(sequencer.AddKeyframe("Events", {1.0f, Variant(ea::string("Shot"))}));
    CHECK(sequencer.Evaluate("Camera", 1.0f).GetFloat() == Catch::Approx(5.0f));

    REQUIRE(sequencer.Play());
    REQUIRE(sequencer.Update(1.1f));
    const ea::vector<SequencerEvent> events = sequencer.ConsumeEvents();
    REQUIRE(events.size() == 1);
    CHECK(events.front().track == "Events");
    CHECK(events.front().value.GetString() == "Shot");
}

TEST_CASE("Sequencer resource round-trips cinematic manifests and validates extensions", "[animation][sequencer][resource]")
{
    const auto context = Tests::GetOrCreateContext(Tests::CreateCompleteContext);
    SequencerResource source(context.Get());
    Sequencer& sequencer = source.GetSequencer();
    sequencer.SetDuration(12.0f);
    sequencer.SetLooping(true);
    REQUIRE(sequencer.AddTrack("Camera", SequencerTrackType::Transform));
    REQUIRE(sequencer.AddTrack("Dialogue", SequencerTrackType::Event));
    REQUIRE(sequencer.GetTrack("Dialogue") != nullptr);
    sequencer.GetTrack("Dialogue")->muted = true;
    REQUIRE(sequencer.AddKeyframe("Camera", {0.0f, Variant(Vector3(0.0f, 1.0f, 2.0f))}));
    REQUIRE(sequencer.AddKeyframe("Camera", {6.0f, Variant(Vector3(4.0f, 3.0f, 8.0f))}));
    REQUIRE(sequencer.AddKeyframe("Dialogue", {3.5f, Variant(ea::string("Hello"))}));

    const JSONValue serialized = source.ToJSON();
    SequencerResource restored(context.Get());
    ea::string error;
    REQUIRE(restored.FromJSON(serialized, &error));
    CHECK(error.empty());
    CHECK(restored.GetSequencer().GetDuration() == Catch::Approx(12.0f));
    CHECK(restored.GetSequencer().IsLooping());
    REQUIRE(restored.GetSequencer().GetTracks().size() == 2);
    const SequencerTrack* restoredDialogue = restored.GetSequencer().GetTrack("Dialogue");
    REQUIRE(restoredDialogue != nullptr);
    CHECK(restoredDialogue->muted);
    REQUIRE(restoredDialogue->keyframes.size() == 1);
    CHECK(restoredDialogue->keyframes.front().value.GetString() == "Hello");
    CHECK(restored.GetSequencer().Evaluate("Camera", 3.0f).GetVector3().x_ == Catch::Approx(2.0f));

    VectorBuffer buffer;
    buffer.SetName("Memory.sequence");
    REQUIRE(source.Save(buffer));
    REQUIRE(buffer.GetSize() > 0);
    buffer.Seek(0);
    SequencerResource loaded(context.Get());
    REQUIRE(loaded.BeginLoad(buffer));
    CHECK(loaded.GetSequencer().GetTracks().size() == 2);

    CHECK(SequencerResource::CheckExtension("cinematic.sequence"));
    CHECK(SequencerResource::CheckExtension("cinematic.sequencer"));
    CHECK_FALSE(SequencerResource::CheckExtension("cinematic.json"));
    CHECK_FALSE(SequencerResource::CheckExtension("cinematic.sequence.bak"));

    JSONValue missingTracks = serialized;
    missingTracks.Erase("tracks");
    REQUIRE_FALSE(restored.FromJSON(missingTracks, &error));
    CHECK_FALSE(error.empty());

    JSONValue invalidDuration = serialized;
    invalidDuration["duration"] = JSONValue(-1.0f);
    REQUIRE_FALSE(restored.FromJSON(invalidDuration, &error));
    CHECK_FALSE(error.empty());

    JSONValue duplicateTrack = serialized;
    JSONValue tracks = duplicateTrack["tracks"];
    REQUIRE(tracks.Size() == 2);
    tracks.Push(tracks[0]);
    duplicateTrack["tracks"] = tracks;
    REQUIRE_FALSE(restored.FromJSON(duplicateTrack, &error));
    CHECK_FALSE(error.empty());
}

TEST_CASE("Blueprint runtime exposes native animation and sequencer nodes", "[animation][blueprint]")
{
    BlueprintRuntime runtime;
    AnimationStateMachine machine;
    Sequencer sequencer;
    runtime.SetAnimationStateMachine(&machine);
    runtime.SetSequencer(&sequencer);

    CHECK(runtime.GetAnimationStateMachine() == &machine);
    CHECK(runtime.GetSequencer() == &sequencer);
    CHECK(runtime.GetRegistry().Find("Anim.PlayStateMachine") != nullptr);
    CHECK(runtime.GetRegistry().Find("Anim.SetParameter") != nullptr);
    CHECK(runtime.GetRegistry().Find("Seq.Play") != nullptr);
    CHECK(runtime.GetRegistry().Find("Seq.Pause") != nullptr);
}
