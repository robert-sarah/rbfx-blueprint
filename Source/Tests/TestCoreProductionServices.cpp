// Copyright (c) 2024-2026 robert-sarah and rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#include <Urho3D/WorldFabric/ContentAddressedCache.h>
#include <Urho3D/WorldFabric/GameplayTestHarness.h>
#include <Urho3D/WorldFabric/HotReloadStateStore.h>
#include <Urho3D/WorldFabric/IncrementalScheduler.h>

#include "CommonUtils.h"

#include <catch2/catch_amalgamated.hpp>

using namespace Urho3D;

TEST_CASE("Incremental scheduler invalidates dependents deterministically", "[worldfabric][scheduler]")
{
    IncrementalScheduler scheduler;
    REQUIRE(scheduler.RegisterTask({"assets", "Import assets", {}, IncrementalTaskState::Clean, 0}));
    REQUIRE(scheduler.RegisterTask({"shaders", "Compile shaders", {}, IncrementalTaskState::Clean, 0}));
    REQUIRE(scheduler.RegisterTask({"package", "Build package", {}, IncrementalTaskState::Clean, 0}));
    REQUIRE(scheduler.AddDependency("shaders", "assets"));
    REQUIRE(scheduler.AddDependency("package", "shaders"));

    ea::string error;
    REQUIRE(scheduler.Invalidate("assets", &error));
    CHECK(scheduler.GetInvalidatedTasks() == ea::vector<ea::string>{"assets", "package", "shaders"});
    CHECK(scheduler.GetReadyTasks() == ea::vector<ea::string>{"assets"});
    REQUIRE(scheduler.MarkSucceeded("assets", &error));
    CHECK(scheduler.GetReadyTasks() == ea::vector<ea::string>{"shaders"});
    REQUIRE(scheduler.MarkSucceeded("shaders", &error));
    CHECK(scheduler.GetReadyTasks() == ea::vector<ea::string>{"package"});
    REQUIRE(scheduler.MarkSucceeded("package", &error));
    CHECK(scheduler.ComputeDigest(&error) != 0);

    IncrementalScheduler cyclic;
    REQUIRE(cyclic.RegisterTask({"a", "A", {}, IncrementalTaskState::Clean, 0}));
    REQUIRE(cyclic.RegisterTask({"b", "B", {}, IncrementalTaskState::Clean, 0}));
    REQUIRE(cyclic.AddDependency("a", "b", &error));
    CHECK_FALSE(cyclic.AddDependency("b", "a", &error));
    CHECK_FALSE(error.empty());
}

TEST_CASE("Content addressed cache replaces artifacts and computes stable digest", "[worldfabric][cache]")
{
    ContentAddressedCache cache;
    CachedArtifact shader{"sha256:shader-1", "shader", "cache/shader-1.bin", 128, 0};
    REQUIRE(cache.Put(shader));
    REQUIRE(cache.Contains(shader.digest));
    CHECK(cache.Get(shader.digest)->revision == 1);

    shader.location = "cache/shader-1-v2.bin";
    REQUIRE(cache.Put(shader));
    CHECK(cache.Get(shader.digest)->revision == 2);
    const unsigned long long digest = cache.ComputeDigest();
    REQUIRE(digest != 0);

    ea::string error;
    CHECK_FALSE(cache.Put({"invalid", "shader", "cache/x", 1, 0}, &error));
    CHECK_FALSE(error.empty());
    REQUIRE(cache.Remove(shader.digest, &error));
    CHECK_FALSE(cache.Contains(shader.digest));
}

TEST_CASE("Gameplay test harness runs callbacks in deterministic order", "[gameplay][tests]")
{
    GameplayTestHarness harness;
    GameplayHarnessTestCase zLast;
    zLast.id = "z-last";
    zLast.description = "Last test";
    zLast.maxFrames = 60;
    zLast.callback = [](void*, unsigned seed, unsigned maxFrames, ea::string*)
    {
        return seed == 42 && maxFrames == 60;
    };
    REQUIRE(harness.Register(zLast));

    GameplayHarnessTestCase aFirst;
    aFirst.id = "a-first";
    aFirst.description = "First test";
    aFirst.maxFrames = 30;
    aFirst.callback = [](void*, unsigned seed, unsigned, ea::string*)
    {
        return seed == 42;
    };
    REQUIRE(harness.Register(aFirst));

    ea::string error;
    const auto results = harness.RunAll(42, &error);
    REQUIRE(results.size() == 2);
    CHECK(results[0].id == "a-first");
    CHECK(results[1].id == "z-last");
    CHECK(harness.WasLastRunSuccessful());
    CHECK(harness.ComputeDigest(&error) != 0);

    GameplayTestHarness failing;
    GameplayHarnessTestCase failure;
    failure.id = "failure";
    failure.description = "Failure";
    failure.maxFrames = 10;
    failure.callback = [](void*, unsigned, unsigned, ea::string* callbackError)
    {
        if (callbackError)
            *callbackError = "intentional failure";
        return false;
    };
    REQUIRE(failing.Register(failure));
    CHECK(failing.RunAll(7, &error).size() == 1);
    CHECK_FALSE(failing.WasLastRunSuccessful());
    CHECK(std::string(error.c_str()) == "Gameplay test failed: failure: intentional failure");
}

TEST_CASE("Hot reload state store preserves generations and fields", "[hotreload][state]")
{
    HotReloadStateStore store;
    StringVariantMap fields;
    fields["health"] = 100;
    fields["name"] = "Player";
    REQUIRE(store.Capture("player", fields));
    CHECK(store.GetGeneration("player") == 1);

    fields["health"] = 80;
    REQUIRE(store.Capture("player", fields));
    CHECK(store.GetGeneration("player") == 2);

    StringVariantMap restored;
    REQUIRE(store.Restore("player", restored));
    CHECK(restored["health"].GetInt() == 80);
    CHECK(restored["name"].GetString() == "Player");
    CHECK(store.ComputeDigest() != 0);

    ea::string error;
    CHECK_FALSE(store.Restore("missing", restored, &error));
    CHECK_FALSE(error.empty());
    REQUIRE(store.Remove("player", &error));
    CHECK(store.GetSize() == 0);
}
