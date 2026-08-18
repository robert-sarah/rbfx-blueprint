// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#include <Urho3D/Scene/WorldPartition.h>
#include <Urho3D/WorldFabric/OpenWorldRuntime.h>
#include <Urho3D/WorldFabric/OpenWorldGameplay.h>

#include <catch2/catch_amalgamated.hpp>

using namespace Urho3D;

TEST_CASE("Open world streaming scheduler consumes deterministic partition work", "[openworld][streaming]")
{
    WorldPartition partition;
    partition.SetStreamingRadius(100.0f);
    partition.SetMaxLoadedCells(2);
    REQUIRE(partition.AddCell({"center", IntVector2{0, 0}, Vector3::ZERO, 20.0f, "center.scene", 128}));
    REQUIRE(partition.AddCell({"east", IntVector2{1, 0}, Vector3{40.0f, 0.0f, 0.0f}, 20.0f, "east.scene", 256}));
    REQUIRE(partition.AddCell({"far", IntVector2{9, 9}, Vector3{1000.0f, 0.0f, 0.0f}, 20.0f, "far.scene", 512}));

    OpenWorldStreamingScheduler scheduler;
    scheduler.SetFrameBudget(2);
    CHECK(scheduler.Update(partition, Vector3::ZERO) == 2);
    CHECK(scheduler.GetPendingCount() == 2);

    OpenWorldCellWork first;
    REQUIRE(scheduler.PopNext(first));
    CHECK(first.cellId == "center");
    CHECK(first.load);
    scheduler.Complete(first.cellId, true, first.estimatedMemoryMb);
    CHECK(scheduler.GetResidentMemoryMb() == 128);

    OpenWorldCellWork second;
    REQUIRE(scheduler.PopNext(second));
    CHECK(second.cellId == "east");
    scheduler.Complete(second.cellId, true, second.estimatedMemoryMb);
    CHECK(scheduler.GetResidentMemoryMb() == 384);
    CHECK(scheduler.GetInFlightCount() == 0);
}

TEST_CASE("Open world occlusion culler applies distance and field of view conservatively", "[openworld][occlusion]")
{
    OpenWorldOcclusionCuller culler;
    REQUIRE(culler.AddProxy({"front", Vector3{0.0f, 0.0f, 10.0f}, Vector3::ONE, true}));
    REQUIRE(culler.AddProxy({"behind", Vector3{0.0f, 0.0f, -10.0f}, Vector3::ONE, true}));
    REQUIRE(culler.AddProxy({"disabled", Vector3{0.0f, 0.0f, 5.0f}, Vector3::ONE, false}));

    const auto visible = culler.QueryVisible(Vector3::ZERO, Vector3::FORWARD, 20.0f, 90.0f);
    REQUIRE(visible.size() == 1);
    CHECK(visible.front() == "front");
    CHECK(culler.RemoveProxy("front"));
    CHECK(culler.GetProxy("front") == nullptr);
}

TEST_CASE("Open world vegetation generation is reproducible per cell", "[openworld][vegetation]")
{
    OpenWorldVegetationSystem vegetation;
    vegetation.SetSeed(1234);
    vegetation.SetDensity(8);
    const auto first = vegetation.GenerateCell("cell-a", Vector3{10.0f, 0.0f, 20.0f});
    const auto second = vegetation.GenerateCell("cell-a", Vector3{10.0f, 0.0f, 20.0f});
    const auto other = vegetation.GenerateCell("cell-b", Vector3{10.0f, 0.0f, 20.0f});

    REQUIRE(first.size() == 8);
    REQUIRE(second.size() == first.size());
    CHECK(first[0].position == second[0].position);
    CHECK(first[0].seed == second[0].seed);
    CHECK(first[0].seed != other[0].seed);
}

TEST_CASE("Open world navigation and character LOD expose cell-local readiness", "[openworld][navigation][lod]")
{
    OpenWorldCellNavigationService navigation;
    REQUIRE(navigation.RegisterCell("cell-a"));
    REQUIRE(navigation.RegisterCell("cell-b"));
    CHECK(navigation.SetState("cell-a", OpenWorldNavigationState::Building));
    CHECK(navigation.SetState("cell-a", OpenWorldNavigationState::Ready, 42));
    CHECK(navigation.SetState("cell-b", OpenWorldNavigationState::Ready, 21));
    CHECK(navigation.GetReadyCells() == std::vector<std::string>{"cell-a", "cell-b"});
    CHECK(navigation.GetCell("cell-a")->polygonCount == 42);
    CHECK(navigation.MarkStale("cell-a"));

    OpenWorldCharacterLODPolicy lod;
    lod.SetDistances(10.0f, 30.0f, 60.0f);
    CHECK(lod.Evaluate(5.0f).level == OpenWorldCharacterLOD::Near);
    CHECK(lod.Evaluate(20.0f).level == OpenWorldCharacterLOD::Mid);
    CHECK(lod.Evaluate(45.0f).level == OpenWorldCharacterLOD::Far);
    CHECK(lod.Evaluate(100.0f).level == OpenWorldCharacterLOD::Culled);
    CHECK(lod.Evaluate(20.0f, false).level == OpenWorldCharacterLOD::Culled);
}

TEST_CASE("Open world weather transitions deterministically between typed states", "[openworld][weather]")
{
    OpenWorldWeatherSystem weather;
    weather.SetSeed(99);
    weather.SetState(OpenWorldWeatherType::Clear, 0.0f, 0.0f, 0.3f);
    REQUIRE(weather.TransitionTo(OpenWorldWeatherType::Storm, 10.0f));
    weather.Update(5.0f);
    CHECK(weather.IsTransitioning());
    CHECK(weather.GetState().intensity == Catch::Approx(0.5f));
    CHECK(weather.GetState().windSpeed == Catch::Approx(11.0f));
    weather.Update(5.0f);
    CHECK_FALSE(weather.IsTransitioning());
    CHECK(weather.GetState().type == OpenWorldWeatherType::Storm);
    CHECK(weather.GetState().humidity == Catch::Approx(0.95f));
}

TEST_CASE("Open world day night cycle wraps and projects daylight", "[openworld][time]")
{
    OpenWorldDayNightCycle cycle;
    cycle.SetDayLength(100.0f);
    cycle.SetTimeOfDay(50.0f);
    CHECK(cycle.GetState().sunElevationDegrees == Catch::Approx(90.0f));
    CHECK(cycle.GetState().daylightFactor == Catch::Approx(1.0f));
    cycle.Advance(55.0f);
    CHECK(cycle.GetTimeOfDay() == Catch::Approx(5.0f));
    CHECK(cycle.GetState().daylightFactor < 0.5f);
}

TEST_CASE("Open world quest service validates definitions and completes required objectives", "[openworld][quest]")
{
    OpenWorldQuestSystem quests;
    OpenWorldQuestDefinition definition;
    definition.id = "first-contact";
    definition.title = "First Contact";
    definition.objectives = {{"meet", "Meet the guide", 1, 0, false}, {"optional", "Find the shrine", 2, 0, true}};
    REQUIRE(quests.Register(definition));
    REQUIRE(quests.Start("first-contact"));
    REQUIRE(quests.AdvanceObjective("first-contact", "optional", 1));
    CHECK(quests.GetState("first-contact")->active);
    REQUIRE(quests.CompleteObjective("first-contact", "meet"));
    CHECK(quests.GetState("first-contact")->completed);
    CHECK(quests.GetCompletedQuestIds() == std::vector<std::string>{"first-contact"});
    CHECK_FALSE(quests.Start("first-contact"));
}

TEST_CASE("Open world save service round-trips versioned state and rejects malformed data", "[openworld][save]")
{
    OpenWorldSaveState source;
    source.playerPosition = Vector3{1.0f, 2.0f, 3.0f};
    source.worldTimeSeconds = 123.5f;
    source.weather.type = OpenWorldWeatherType::Rain;
    source.weather.intensity = 0.7f;
    source.worldRevision = 44;
    source.changedCells = {"cell-b", "cell-a", "cell-a"};
    source.quests.push_back({"quest", {{"objective", "Do it", 3, 2, false}}, true, false, 7});

    const std::string serialized = OpenWorldSaveService::Serialize(source);
    OpenWorldSaveState restored;
    std::string error;
    REQUIRE(OpenWorldSaveService::Deserialize(serialized, restored, &error));
    CHECK(error.empty());
    CHECK(restored.playerPosition == source.playerPosition);
    CHECK(restored.worldTimeSeconds == Catch::Approx(source.worldTimeSeconds));
    CHECK(restored.weather.type == source.weather.type);
    CHECK(restored.changedCells == std::vector<std::string>{"cell-a", "cell-b"});
    CHECK(restored.quests.size() == 1);
    CHECK_FALSE(OpenWorldSaveService::Deserialize("{\"version\":99}", restored, &error));
    CHECK_FALSE(error.empty());
}

TEST_CASE("Open world spatial audio resolves priority and falloff", "[openworld][audio]")
{
    OpenWorldSpatialAudioService audio;
    REQUIRE(audio.AddZone({"forest", Vector3::ZERO, Vector3{2.0f, 2.0f, 2.0f}, 1, 0.8f, 0.6f, 4.0f, "forest.ogg"}));
    REQUIRE(audio.AddZone({"cave", Vector3{1.0f, 0.0f, 0.0f}, Vector3::ONE, 2, 1.0f, 0.5f, 2.0f, "cave.ogg"}));
    const OpenWorldAudioMix inside = audio.Evaluate(Vector3{1.0f, 0.0f, 0.0f});
    CHECK(inside.zoneId == "cave");
    CHECK(inside.influence == Catch::Approx(1.0f));
    CHECK(inside.ambientSound == "cave.ogg");
    CHECK(audio.Evaluate(Vector3{100.0f, 0.0f, 0.0f}).zoneId.empty());
}

TEST_CASE("Open world runtime services compose ticking and persistence state", "[openworld][integration]")
{
    WorldPartition partition;
    partition.SetStreamingRadius(50.0f);
    partition.SetMaxLoadedCells(1);
    REQUIRE(partition.AddCell({"home", IntVector2{0, 0}, Vector3::ZERO, 10.0f, "home.scene", 32}));

    OpenWorldRuntimeServices services;
    OpenWorldQuestDefinition quest;
    quest.id = "intro";
    quest.title = "Intro";
    quest.objectives = {{"start", "Start", 1, 0, false}};
    REQUIRE(services.GetQuests().Register(quest));
    REQUIRE(services.GetQuests().Start("intro"));
    services.GetWeather().SetState(OpenWorldWeatherType::Rain, 0.5f, 4.0f, 0.8f);
    services.GetDayNight().SetDayLength(100.0f);
    services.GetDayNight().SetTimeOfDay(10.0f);

    CHECK(services.Update(partition, Vector3::ZERO, 2.0f) == 1);
    const OpenWorldSaveState captured = services.CaptureSaveState(Vector3{4.0f, 5.0f, 6.0f}, 7, {"home"});
    CHECK(captured.worldTimeSeconds == Catch::Approx(12.0f));
    CHECK(captured.quests.size() == 1);

    OpenWorldRuntimeServices restored;
    REQUIRE(restored.GetQuests().Register(quest));
    CHECK(restored.RestoreSaveState(captured));
    CHECK(restored.GetWeather().GetState().type == OpenWorldWeatherType::Rain);
    CHECK(restored.GetDayNight().GetTimeOfDay() == Catch::Approx(12.0f));
}
