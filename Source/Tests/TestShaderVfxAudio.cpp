// SPDX-License-Identifier: MIT

#include <Urho3D/Audio/AudioMixer.h>
#include <Urho3D/Audio/AudioMixerResource.h>
#include <Urho3D/Blueprint/BlueprintRuntime.h>
#include <Urho3D/Particles/VFXGraph.h>
#include <Urho3D/Particles/VFXGraphResource.h>
#include <Urho3D/Shader/ShaderGraph.h>
#include <Urho3D/Shader/ShaderGraphResource.h>

#include "CommonUtils.h"

#include <catch2/catch_amalgamated.hpp>

using namespace Urho3D;

TEST_CASE("ShaderGraph validates connections and generates GLSL and HLSL", "[shader-graph]")
{
    ShaderGraph graph;
    REQUIRE(graph.SetParameter({"Tint", ShaderGraphValueType::Color, Variant(Color::WHITE)}));
    const unsigned parameter = graph.AddNode("Tint", ShaderGraphNodeKind::Parameter, ShaderGraphValueType::Color, Variant(ea::string("Tint")));
    const unsigned output = graph.AddNode("Output", ShaderGraphNodeKind::Output, ShaderGraphValueType::Color);
    REQUIRE(graph.Connect(parameter, "value", output, "color"));
    REQUIRE(graph.SetOutputNode(output));
    ea::string error;
    REQUIRE(graph.Validate(&error));
    const ea::string glsl = graph.GenerateGLSL(&error);
    const ea::string hlsl = graph.GenerateHLSL(&error);
    CHECK(glsl.find("uniform vec4 u_Tint") != ea::string::npos);
    CHECK(glsl.find("fragColor") != ea::string::npos);
    CHECK(hlsl.find("float4 u_Tint") != ea::string::npos);
    CHECK(hlsl.find("SV_Target") != ea::string::npos);
}

TEST_CASE("ShaderGraphResource round-trips deterministic JSON assets", "[shader-graph][resource]")
{
    auto context = Tests::GetOrCreateContext(Tests::CreateCompleteContext);
    auto resource = MakeShared<ShaderGraphResource>(context);
    auto& graph = resource->GetGraph();
    graph.SetParameter({"Tint", ShaderGraphValueType::Color, Variant(Color::WHITE)});
    const unsigned parameter = graph.AddNode("Tint", ShaderGraphNodeKind::Parameter, ShaderGraphValueType::Color,
        Variant(ea::string("Tint")));
    const unsigned output = graph.AddNode("Output", ShaderGraphNodeKind::Output, ShaderGraphValueType::Color);
    REQUIRE(graph.Connect(parameter, "value", output, "color"));
    REQUIRE(graph.SetOutputNode(output));

    const JSONValue serialized = resource->ToJSON();
    CHECK(ShaderGraphResource::CheckExtension("Materials/Test.shadergraph"));
    CHECK_FALSE(ShaderGraphResource::CheckExtension("Materials/Test.material"));

    auto loaded = MakeShared<ShaderGraphResource>(context);
    ea::string error;
    REQUIRE(loaded->FromJSON(serialized, &error));
    CHECK(loaded->ToJSON() == serialized);
    CHECK(loaded->GetGraph().GenerateGLSL(&error).find("fragColor") != ea::string::npos);

    JSONValue invalid = serialized;
    JSONValue duplicateConnections(JSON_ARRAY);
    duplicateConnections.Push(serialized.Get("connections").GetArray()[0]);
    duplicateConnections.Push(serialized.Get("connections").GetArray()[0]);
    invalid.Set("connections", ea::move(duplicateConnections));
    CHECK_FALSE(loaded->FromJSON(invalid, &error));
}

TEST_CASE("ShaderGraph rejects missing parameters and duplicate output nodes", "[shader-graph]")
{
    ShaderGraph missingParameter;
    const unsigned parameter = missingParameter.AddNode("Missing", ShaderGraphNodeKind::Parameter,
        ShaderGraphValueType::Color, Variant(ea::string("Missing")));
    const unsigned output = missingParameter.AddNode("Output", ShaderGraphNodeKind::Output, ShaderGraphValueType::Color);
    REQUIRE(missingParameter.Connect(parameter, "value", output, "color"));
    REQUIRE(missingParameter.SetOutputNode(output));
    ea::string error;
    CHECK_FALSE(missingParameter.Validate(&error));
    CHECK(error.find("missing parameter") != ea::string::npos);

    ShaderGraph duplicateOutputs;
    const unsigned firstOutput = duplicateOutputs.AddNode("First", ShaderGraphNodeKind::Output, ShaderGraphValueType::Color);
    duplicateOutputs.AddNode("Second", ShaderGraphNodeKind::Output, ShaderGraphValueType::Color);
    REQUIRE(duplicateOutputs.SetOutputNode(firstOutput));
    error.clear();
    CHECK_FALSE(duplicateOutputs.Validate(&error));
    CHECK(error.find("exactly one Output") != ea::string::npos);
}

TEST_CASE("ShaderGraph emits HLSL sampler state for texture parameters", "[shader-graph]")
{
    ShaderGraph graph;
    REQUIRE(graph.SetParameter({"Albedo", ShaderGraphValueType::Texture2D, Variant()}));
    const unsigned texture = graph.AddNode("Albedo", ShaderGraphNodeKind::TextureSample,
        ShaderGraphValueType::Color, Variant(ea::string("Albedo")));
    const unsigned output = graph.AddNode("Output", ShaderGraphNodeKind::Output, ShaderGraphValueType::Color);
    REQUIRE(graph.Connect(texture, "color", output, "color"));
    REQUIRE(graph.SetOutputNode(output));
    ea::string error;
    const ea::string hlsl = graph.GenerateHLSL(&error);
    REQUIRE_FALSE(hlsl.empty());
    CHECK(hlsl.find("Texture2D u_Albedo") != ea::string::npos);
    CHECK(hlsl.find("SamplerState u_AlbedoSampler") != ea::string::npos);
}

TEST_CASE("ShaderGraph rejects cycles and duplicate input connections", "[shader-graph]")
{
    ShaderGraph graph;
    const unsigned add = graph.AddNode("Add", ShaderGraphNodeKind::Add, ShaderGraphValueType::Float);
    const unsigned output = graph.AddNode("Output", ShaderGraphNodeKind::Output, ShaderGraphValueType::Float);
    REQUIRE(graph.Connect(add, "value", output, "color"));
    CHECK_FALSE(graph.Connect(add, "other", output, "color"));
    CHECK_FALSE(graph.Connect(add, "a", add, "b"));
    REQUIRE(graph.SetOutputNode(output));
    ea::string error;
    CHECK(graph.Validate(&error));
}

TEST_CASE("VFXGraph compiles, emits bounded particles and ribbon points", "[vfx-graph]")
{
    VFXGraph graph;
    const unsigned output = graph.AddNode("Output", VFXNodeType::Output);
    REQUIRE(graph.SetOutputNode(output));
    graph.SetSimulationMode(VFXSimulationMode::GPU);
    graph.SetMaxParticles(4);
    graph.SetSpawnRate(20.0f);
    graph.SetParticleLifetime(2.0f);
    graph.SetInitialVelocity(Vector3(1.0f, 0.0f, 0.0f));
    graph.SetForce(Vector3(0.0f, -1.0f, 0.0f));
    graph.SetRibbonTrailLength(3);
    REQUIRE(graph.Play());
    graph.Update(0.25f);
    CHECK(graph.GetSimulationMode() == VFXSimulationMode::GPU);
    CHECK(graph.GetParticles().size() == 4);
    CHECK(graph.GetRibbonPoints().size() == 1);
    graph.Update(0.25f);
    CHECK(graph.GetParticles().size() <= 4);
    CHECK(graph.GetRibbonPoints().size() <= 3);
    REQUIRE(graph.Stop());
}

TEST_CASE("VFXGraphResource round-trips deterministic JSON assets", "[vfx-graph][resource]")
{
    auto context = Tests::GetOrCreateContext(Tests::CreateCompleteContext);
    auto resource = MakeShared<VFXGraphResource>(context);
    VFXGraph& graph = resource->GetGraph();
    const unsigned force = graph.AddNode("Force", VFXNodeType::Force);
    graph.GetNode(force)->vectorValue = Vector3(0.0f, -9.8f, 0.0f);
    graph.GetNode(force)->scalarValue = 0.5f;
    const unsigned output = graph.AddNode("Output", VFXNodeType::Output);
    REQUIRE(graph.SetOutputNode(output));
    graph.SetSimulationMode(VFXSimulationMode::GPU);
    graph.SetMaxParticles(128);
    graph.SetSpawnRate(24.0f);
    graph.SetParticleLifetime(3.0f);
    graph.SetInitialVelocity(Vector3(1.0f, 2.0f, 3.0f));
    graph.SetForce(Vector3(0.0f, -9.8f, 0.0f));
    graph.SetDrag(0.25f);
    graph.SetRibbonTrailLength(12);

    const JSONValue serialized = resource->ToJSON();
    CHECK(VFXGraphResource::CheckExtension("Effects/Smoke.vfxgraph"));
    CHECK_FALSE(VFXGraphResource::CheckExtension("Effects/Smoke.xml"));

    auto loaded = MakeShared<VFXGraphResource>(context);
    ea::string error;
    REQUIRE(loaded->FromJSON(serialized, &error));
    CHECK(loaded->ToJSON() == serialized);
    CHECK(loaded->GetGraph().Compile(&error));

    JSONValue invalid = serialized;
    JSONValue duplicateNodes(JSON_ARRAY);
    duplicateNodes.Push(serialized.Get("nodes").GetArray()[0]);
    duplicateNodes.Push(serialized.Get("nodes").GetArray()[0]);
    invalid.Set("nodes", ea::move(duplicateNodes));
    CHECK_FALSE(loaded->FromJSON(invalid, &error));
}

TEST_CASE("AudioMixer routes voices through hierarchical buses and meters", "[audio-mixer]")
{
    AudioMixer mixer;
    REQUIRE(mixer.AddBus({"Master", {}, 0.8f, false, false, {}}));
    REQUIRE(mixer.AddBus({"SFX", "Master", 0.5f, false, false, {}}));
    REQUIRE(mixer.AddEffect("SFX", {AudioDspType::Compressor, true, 0.75f, 2.0f, 0.1f}));
    REQUIRE(mixer.AddVoice({"shot", "SFX", 1.0f, 0.0f, 1.0f, 1.0f, true}));
    CHECK(mixer.GetEffectiveBusVolume("SFX") == Catch::Approx(0.4f));
    mixer.Update(0.016f);
    const AudioMeter meter = mixer.GetBusMeter("SFX");
    CHECK(meter.activeVoices == 1);
    CHECK(meter.peak == Catch::Approx(0.4f));
    REQUIRE(mixer.SetBusMuted("SFX", true));
    CHECK(mixer.GetEffectiveBusVolume("SFX") == 0.0f);
    REQUIRE(mixer.SetBusMuted("SFX", false));
    REQUIRE(mixer.SetBusSoloed("SFX", true));
    CHECK(mixer.GetEffectiveBusVolume("Master") == 0.0f);
}

TEST_CASE("AudioMixerResource round-trips buses effects and voices", "[audio-mixer][resource]")
{
    auto context = Tests::GetOrCreateContext(Tests::CreateCompleteContext);
    auto resource = MakeShared<AudioMixerResource>(context);
    AudioMixer& mixer = resource->GetMixer();
    REQUIRE(mixer.AddBus({"Master", {}, 1.0f, false, false, {}}));
    REQUIRE(mixer.AddBus({"SFX", "Master", 0.75f, false, false, {}}));
    REQUIRE(mixer.AddEffect("SFX", {AudioDspType::Reverb, true, 0.6f, 0.2f, 0.8f}));
    REQUIRE(mixer.AddVoice({"Explosion", "SFX", 1.2f, -0.25f, 1.1f, 0.9f, true}));

    const JSONValue serialized = resource->ToJSON();
    CHECK(AudioMixerResource::CheckExtension("Audio/Main.audiomixer"));
    CHECK_FALSE(AudioMixerResource::CheckExtension("Audio/Main.wav"));

    auto loaded = MakeShared<AudioMixerResource>(context);
    ea::string error;
    REQUIRE(loaded->FromJSON(serialized, &error));
    CHECK(loaded->ToJSON() == serialized);
    CHECK(loaded->GetMixer().GetBus("SFX")->effects.size() == 1);
    CHECK(loaded->GetMixer().GetVoice("Explosion") != nullptr);

    JSONValue invalid = serialized;
    JSONValue invalidVoices(JSON_ARRAY);
    JSONValue voice = serialized.Get("voices").GetArray()[0];
    voice.Set("bus", "MissingBus");
    invalidVoices.Push(ea::move(voice));
    invalid.Set("voices", ea::move(invalidVoices));
    CHECK_FALSE(loaded->FromJSON(invalid, &error));
}

TEST_CASE("Blueprint runtime exposes ShaderGraph, VFXGraph and AudioMixer nodes", "[production][blueprint]")
{
    BlueprintRuntime runtime;
    ShaderGraph shaderGraph;
    VFXGraph vfxGraph;
    AudioMixer audioMixer;
    runtime.SetShaderGraph(&shaderGraph);
    runtime.SetVFXGraph(&vfxGraph);
    runtime.SetAudioMixer(&audioMixer);
    CHECK(runtime.GetShaderGraph() == &shaderGraph);
    CHECK(runtime.GetVFXGraph() == &vfxGraph);
    CHECK(runtime.GetAudioMixer() == &audioMixer);
    CHECK(runtime.GetRegistry().Find("Shader.SetGraphParameter") != nullptr);
    CHECK(runtime.GetRegistry().Find("VFX.Play") != nullptr);
    CHECK(runtime.GetRegistry().Find("VFX.Stop") != nullptr);
    CHECK(runtime.GetRegistry().Find("Audio.SetBusVolume") != nullptr);
}
