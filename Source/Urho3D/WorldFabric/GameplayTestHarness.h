// Copyright (c) 2024-2026 robert-sarah and rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <EASTL/string.h>
#include <Urho3D/Core/Attribute.h>

#include <string>

#include <memory>
#include <vector>

namespace Urho3D
{

using GameplayHarnessTestCallback = bool (*)(void* userData, unsigned seed, unsigned maxFrames, ea::string* error);

struct GameplayHarnessTestCase
{
    std::string id;
    std::string description;
    unsigned maxFrames{600};
    GameplayHarnessTestCallback callback{};
    void* userData{};
};

struct GameplayHarnessTestResult
{
    std::string id;
    bool passed{};
    unsigned seed{};
    unsigned maxFrames{};
    std::string error;
};

/// Deterministic gameplay test harness suitable for local runs and CI.
class GameplayTestHarness
{
public:
    GameplayTestHarness();
    ~GameplayTestHarness();
    GameplayTestHarness(const GameplayTestHarness&) = delete;
    GameplayTestHarness& operator=(const GameplayTestHarness&) = delete;

    bool Register(const GameplayHarnessTestCase& test, ea::string* error = nullptr);
    bool Unregister(const ea::string& id, ea::string* error = nullptr);
    void Clear();

    std::vector<GameplayHarnessTestResult> RunAll(unsigned seed, ea::string* error = nullptr) const;
    const GameplayHarnessTestCase* GetTest(const ea::string& id) const;
    unsigned GetTestCount() const;
    const std::vector<GameplayHarnessTestResult>& GetLastResults() const;
    bool WasLastRunSuccessful() const;
    unsigned long long ComputeDigest(ea::string* error = nullptr) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}
