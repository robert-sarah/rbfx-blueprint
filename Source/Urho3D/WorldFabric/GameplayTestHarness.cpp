// Copyright (c) 2024-2026 robert-sarah and rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#include "GameplayTestHarness.h"

#include <Urho3D/Core/StringUtils.h>

#include <algorithm>
#include <cstdio>

namespace Urho3D
{

struct GameplayTestHarness::Impl
{
    std::vector<std::unique_ptr<GameplayHarnessTestCase>> tests;
    mutable std::vector<GameplayHarnessTestResult> lastResults;
};

namespace
{

void SetError(ea::string* error, const ea::string& message)
{
    if (error)
        *error = message;
}

unsigned long long HashText(const ea::string& text)
{
    unsigned long long digest = 1469598103934665603ull;
    for (const unsigned char character : text)
    {
        digest ^= character;
        digest *= 1099511628211ull;
    }
    return digest;
}

}

GameplayTestHarness::GameplayTestHarness()
{
    std::fprintf(stderr, "harness: constructor begin\\n");
    impl_ = std::make_unique<Impl>();
    std::fprintf(stderr, "harness: constructor end\\n");
}

GameplayTestHarness::~GameplayTestHarness() = default;

bool GameplayTestHarness::Register(const GameplayHarnessTestCase& test, ea::string* error)
{
    if (test.id.empty())
    {
        SetError(error, "Gameplay test id must not be empty");
        return false;
    }
    if (!test.callback)
    {
        SetError(error, ea::string("Gameplay test callback must not be empty: ") + test.id.c_str());
        return false;
    }
    if (test.maxFrames == 0)
    {
        SetError(error, ea::string("Gameplay test maxFrames must be greater than zero: ") + test.id.c_str());
        return false;
    }
    if (std::any_of(impl_->tests.begin(), impl_->tests.end(), [&test](const std::unique_ptr<GameplayHarnessTestCase>& candidate)
        {
            return candidate && candidate->id == test.id;
        }))
    {
        SetError(error, ea::string("Gameplay test is already registered: ") + test.id.c_str());
        return false;
    }

    auto owned = std::make_unique<GameplayHarnessTestCase>();
    owned->id = test.id;
    owned->description = test.description;
    owned->maxFrames = test.maxFrames;
    owned->callback = test.callback;
    owned->userData = test.userData;
    impl_->tests.push_back(std::move(owned));
    return true;
}

bool GameplayTestHarness::Unregister(const ea::string& id, ea::string* error)
{
    const auto it = std::find_if(impl_->tests.begin(), impl_->tests.end(), [&id](const std::unique_ptr<GameplayHarnessTestCase>& test)
    {
        return test && test->id == id.c_str();
    });
    if (it == impl_->tests.end())
    {
        SetError(error, ea::string("Gameplay test was not found: ") + id.c_str());
        return false;
    }
    impl_->tests.erase(it);
    return true;
}

void GameplayTestHarness::Clear()
{
    impl_->tests.clear();
    impl_->lastResults.clear();
}

unsigned GameplayTestHarness::GetTestCount() const
{
    return static_cast<unsigned>(impl_->tests.size());
}

const std::vector<GameplayHarnessTestResult>& GameplayTestHarness::GetLastResults() const
{
    return impl_->lastResults;
}

const GameplayHarnessTestCase* GameplayTestHarness::GetTest(const ea::string& id) const
{
    const auto it = std::find_if(impl_->tests.begin(), impl_->tests.end(), [&id](const std::unique_ptr<GameplayHarnessTestCase>& test)
    {
        return test && test->id == id.c_str();
    });
    return it != impl_->tests.end() ? it->get() : nullptr;
}

std::vector<GameplayHarnessTestResult> GameplayTestHarness::RunAll(unsigned seed, ea::string* error) const
{
    std::vector<const GameplayHarnessTestCase*> ordered;
    ordered.reserve(impl_->tests.size());
    for (const std::unique_ptr<GameplayHarnessTestCase>& test : impl_->tests)
    {
        if (test)
            ordered.push_back(test.get());
    }
    std::sort(ordered.begin(), ordered.end(), [](const GameplayHarnessTestCase* lhs, const GameplayHarnessTestCase* rhs)
    {
        return lhs->id < rhs->id;
    });

    impl_->lastResults.clear();
    for (const GameplayHarnessTestCase* test : ordered)
    {
        GameplayHarnessTestResult result;
        result.id = test->id;
        result.seed = seed;
        result.maxFrames = test->maxFrames;
        ea::string callbackError;
        result.passed = test->callback(test->userData, seed, test->maxFrames, &callbackError);
        result.error = callbackError.c_str();
        if (!result.passed && result.error.empty())
            result.error = "Gameplay test returned false";
        impl_->lastResults.push_back(result);
    }

    if (!WasLastRunSuccessful())
    {
        for (const GameplayHarnessTestResult& result : impl_->lastResults)
        {
            if (!result.passed)
            {
                ea::string message = "Gameplay test failed: ";
                message += result.id.c_str();
                message += ": ";
                message += result.error.c_str();
                SetError(error, message);
                break;
            }
        }
    }
    return impl_->lastResults;
}

bool GameplayTestHarness::WasLastRunSuccessful() const
{
    return std::all_of(impl_->lastResults.begin(), impl_->lastResults.end(), [](const GameplayHarnessTestResult& result)
    {
        return result.passed;
    });
}

unsigned long long GameplayTestHarness::ComputeDigest(ea::string* error) const
{
    for (const std::unique_ptr<GameplayHarnessTestCase>& test : impl_->tests)
    {
        if (!test || test->id.empty() || !test->callback || test->maxFrames == 0)
        {
            SetError(error, "Gameplay test registry is invalid");
            return 0;
        }
    }

    std::vector<std::string> ids;
    ids.reserve(impl_->tests.size());
    for (const std::unique_ptr<GameplayHarnessTestCase>& test : impl_->tests)
        ids.push_back(test->id);
    std::sort(ids.begin(), ids.end());

    ea::string serialized = "gameplay-tests-v1\\n";
    for (const std::string& id : ids)
    {
        const GameplayHarnessTestCase* test = GetTest(ea::string(id.c_str()));
        serialized += test->id.c_str();
        serialized += "|";
        serialized += test->description.c_str();
        serialized += "|";
        serialized += ToString("%u", test->maxFrames);
        serialized += "\\n";
    }
    for (const GameplayHarnessTestResult& result : impl_->lastResults)
    {
        serialized += result.id.c_str();
        serialized += result.passed ? "|pass|" : "|fail|";
        serialized += ToString("%u", result.seed);
        serialized += "|";
        serialized += ToString("%u", result.maxFrames);
        serialized += "|";
        serialized += result.error.c_str();
        serialized += "\\n";
    }
    return HashText(serialized);
}

}
