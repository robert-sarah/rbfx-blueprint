// Copyright (c) 2026 the rbfx-blueprint project.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <Urho3D/Urho3D.h>
#include <Urho3D/Core/Variant.h>

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Urho3D
{

enum class DeterministicTimeMachineDomain
{
    Gameplay,
    Physics,
    AI,
    Network,
    Blueprint,
    RbScript,
    Custom
};

struct URHO3D_API DeterministicTimeMachineFrame
{
    unsigned frame{};
    unsigned generation{};
    DeterministicTimeMachineDomain domain{DeterministicTimeMachineDomain::Gameplay};
    StringVariantMap state;
    StringVariantMap input;
    unsigned long long digest{};
    std::string label;
};

struct URHO3D_API DeterministicFrameDifference
{
    bool valid{};
    unsigned fromFrame{};
    unsigned toFrame{};
    unsigned long long fromDigest{};
    unsigned long long toDigest{};
    std::vector<std::string> changedKeys;
};

using UniversalDeterministicStep = std::function<bool(unsigned frame, float fixedDelta,
    const StringVariantMap& input, const StringVariantMap& currentState, StringVariantMap& nextState)>;

/// Branchable deterministic history for gameplay, networking, scripts, physics and replays.
class URHO3D_API UniversalDeterministicTimeMachine
{
public:
    explicit UniversalDeterministicTimeMachine(unsigned capacity = 256);
    ~UniversalDeterministicTimeMachine() = default;

    void Configure(float fixedDelta, unsigned capacity);
    float GetFixedDelta() const { return fixedDelta_; }
    unsigned GetCapacity() const { return capacity_; }
    unsigned GetCurrentFrame() const { return currentFrame_; }
    unsigned GetGeneration() const { return generation_; }
    const std::string& GetCurrentBranch() const { return currentBranch_; }
    const StringVariantMap& GetState() const { return state_; }

    bool Start(const StringVariantMap& initialState, const std::string& branch = "main",
        DeterministicTimeMachineDomain domain = DeterministicTimeMachineDomain::Gameplay,
        const std::string& label = "Initial state");
    bool Advance(const StringVariantMap& input, const UniversalDeterministicStep& step,
        DeterministicTimeMachineDomain domain = DeterministicTimeMachineDomain::Gameplay,
        const std::string& label = {});
    bool Restore(unsigned frame);
    bool ReplayTo(unsigned targetFrame, const UniversalDeterministicStep& step);

    /// Create a copy-on-write style branch by copying retained frames through a checkpoint.
    bool CreateBranch(const std::string& branch, unsigned fromFrame, std::string* error = nullptr);
    bool SwitchBranch(const std::string& branch, std::string* error = nullptr);
    bool HasBranch(const std::string& branch) const;
    std::vector<std::string> GetBranches() const;

    const DeterministicTimeMachineFrame* FindFrame(unsigned frame) const;
    const DeterministicTimeMachineFrame* FindFrame(const std::string& branch, unsigned frame) const;
    const std::vector<DeterministicTimeMachineFrame>& GetFrames() const;
    DeterministicFrameDifference CompareFrames(unsigned fromFrame, unsigned toFrame) const;
    DeterministicFrameDifference CompareFrames(const std::string& leftBranch, unsigned leftFrame,
        const std::string& rightBranch, unsigned rightFrame) const;
    bool FindFirstDivergence(const std::string& leftBranch, const std::string& rightBranch,
        unsigned& frame, DeterministicFrameDifference& difference) const;

    unsigned long long ComputeDigest() const;
    unsigned long long ComputeBranchDigest(const std::string& branch) const;
    void Clear();

private:
    struct Branch
    {
        std::vector<DeterministicTimeMachineFrame> frames;
    };

    static unsigned long long ComputeStateDigest(const StringVariantMap& state);
    static DeterministicFrameDifference Compare(const DeterministicTimeMachineFrame* left,
        const DeterministicTimeMachineFrame* right);
    static const DeterministicTimeMachineFrame* FindFrame(const Branch& branch, unsigned frame);
    void TrimBranch(Branch& branch);
    bool ApplyFrame(unsigned frame, const StringVariantMap& input, const UniversalDeterministicStep& step,
        DeterministicTimeMachineDomain domain, const std::string& label);
    void SetError(std::string* error, const std::string& message) const;

    float fixedDelta_{1.0f / 60.0f};
    unsigned capacity_{256};
    unsigned currentFrame_{};
    unsigned generation_{1};
    std::string currentBranch_{"main"};
    StringVariantMap state_;
    std::unordered_map<std::string, Branch> branches_;
};

} // namespace Urho3D
