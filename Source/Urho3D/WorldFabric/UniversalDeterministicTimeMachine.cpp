// Copyright (c) 2026 the rbfx-blueprint project.
//
// SPDX-License-Identifier: MIT
//

#include "UniversalDeterministicTimeMachine.h"

#include <algorithm>
#include <cstdint>
#include <utility>

namespace Urho3D
{

namespace
{

constexpr std::uint64_t FnvOffset = 1469598103934665603ull;
constexpr std::uint64_t FnvPrime = 1099511628211ull;

void HashByte(std::uint64_t& hash, unsigned char value)
{
    hash ^= value;
    hash *= FnvPrime;
}

void HashValue(std::uint64_t& hash, std::uint64_t value)
{
    for (unsigned i = 0; i < sizeof(value); ++i)
        HashByte(hash, static_cast<unsigned char>((value >> (i * 8)) & 0xff));
}

void HashString(std::uint64_t& hash, const ea::string& value)
{
    HashValue(hash, static_cast<std::uint64_t>(value.size()));
    for (unsigned char byte : value)
        HashByte(hash, byte);
}

} // namespace

UniversalDeterministicTimeMachine::UniversalDeterministicTimeMachine(unsigned capacity)
{
    Configure(fixedDelta_, capacity);
}

void UniversalDeterministicTimeMachine::Configure(float fixedDelta, unsigned capacity)
{
    fixedDelta_ = fixedDelta > 0.0f ? fixedDelta : 1.0f / 60.0f;
    capacity_ = std::max(2u, capacity);
    for (auto& entry : branches_)
        TrimBranch(entry.second);
}

bool UniversalDeterministicTimeMachine::Start(const StringVariantMap& initialState, const std::string& branch,
    DeterministicTimeMachineDomain domain, const std::string& label)
{
    currentBranch_ = branch.empty() ? "main" : branch;
    branches_.clear();
    state_ = initialState;
    currentFrame_ = 0;
    generation_ = 1;

    DeterministicTimeMachineFrame initial;
    initial.frame = 0;
    initial.generation = generation_;
    initial.domain = domain;
    initial.state = initialState;
    initial.digest = ComputeStateDigest(initialState);
    initial.label = label;
    branches_[currentBranch_].frames.push_back(std::move(initial));
    return true;
}

bool UniversalDeterministicTimeMachine::Advance(const StringVariantMap& input,
    const UniversalDeterministicStep& step, DeterministicTimeMachineDomain domain, const std::string& label)
{
    if (!step || branches_.find(currentBranch_) == branches_.end())
        return false;
    return ApplyFrame(currentFrame_ + 1, input, step, domain, label);
}

bool UniversalDeterministicTimeMachine::ApplyFrame(unsigned frame, const StringVariantMap& input,
    const UniversalDeterministicStep& step, DeterministicTimeMachineDomain domain, const std::string& label)
{
    auto branchIt = branches_.find(currentBranch_);
    if (branchIt == branches_.end() || !step || frame <= currentFrame_)
        return false;

    auto& frames = branchIt->second.frames;
    frames.erase(std::remove_if(frames.begin(), frames.end(), [this](const DeterministicTimeMachineFrame& item) {
        return item.frame > currentFrame_;
    }), frames.end());

    StringVariantMap nextState = state_;
    if (!step(frame, fixedDelta_, input, state_, nextState))
        return false;

    ++generation_;
    state_ = nextState;
    currentFrame_ = frame;

    DeterministicTimeMachineFrame next;
    next.frame = frame;
    next.generation = generation_;
    next.domain = domain;
    next.state = state_;
    next.input = input;
    next.digest = ComputeStateDigest(state_);
    next.label = label;
    frames.push_back(std::move(next));
    TrimBranch(branchIt->second);
    return true;
}

bool UniversalDeterministicTimeMachine::Restore(unsigned frame)
{
    const DeterministicTimeMachineFrame* snapshot = FindFrame(frame);
    if (!snapshot)
        return false;
    state_ = snapshot->state;
    currentFrame_ = snapshot->frame;
    ++generation_;
    return true;
}

bool UniversalDeterministicTimeMachine::ReplayTo(unsigned targetFrame, const UniversalDeterministicStep& step)
{
    if (!step || targetFrame < currentFrame_)
        return false;

    const auto branchIt = branches_.find(currentBranch_);
    if (branchIt == branches_.end())
        return false;

    std::vector<DeterministicTimeMachineFrame> sourceFrames;
    for (const DeterministicTimeMachineFrame& frame : branchIt->second.frames)
    {
        if (frame.frame > currentFrame_ && frame.frame <= targetFrame)
            sourceFrames.push_back(frame);
    }
    if (sourceFrames.size() != targetFrame - currentFrame_)
        return false;

    for (const DeterministicTimeMachineFrame& source : sourceFrames)
    {
        if (!ApplyFrame(source.frame, source.input, step, source.domain, source.label))
            return false;
    }
    return true;
}

bool UniversalDeterministicTimeMachine::CreateBranch(const std::string& branch, unsigned fromFrame,
    std::string* error)
{
    if (branch.empty())
    {
        SetError(error, "A deterministic branch requires a non-empty name.");
        return false;
    }
    if (HasBranch(branch))
    {
        SetError(error, "The deterministic branch already exists.");
        return false;
    }

    const auto sourceIt = branches_.find(currentBranch_);
    if (sourceIt == branches_.end() || !FindFrame(sourceIt->second, fromFrame))
    {
        SetError(error, "The requested source frame is not retained in the current branch.");
        return false;
    }

    Branch copy;
    for (const DeterministicTimeMachineFrame& frame : sourceIt->second.frames)
    {
        if (frame.frame <= fromFrame)
            copy.frames.push_back(frame);
    }
    branches_.emplace(branch, std::move(copy));
    currentBranch_ = branch;
    const DeterministicTimeMachineFrame* checkpoint = FindFrame(fromFrame);
    state_ = checkpoint->state;
    currentFrame_ = fromFrame;
    ++generation_;
    return true;
}

bool UniversalDeterministicTimeMachine::SwitchBranch(const std::string& branch, std::string* error)
{
    const auto branchIt = branches_.find(branch);
    if (branchIt == branches_.end() || branchIt->second.frames.empty())
    {
        SetError(error, "The deterministic branch does not exist or contains no frames.");
        return false;
    }
    currentBranch_ = branch;
    const DeterministicTimeMachineFrame& last = branchIt->second.frames.back();
    state_ = last.state;
    currentFrame_ = last.frame;
    ++generation_;
    return true;
}

bool UniversalDeterministicTimeMachine::HasBranch(const std::string& branch) const
{
    return branches_.find(branch) != branches_.end();
}

std::vector<std::string> UniversalDeterministicTimeMachine::GetBranches() const
{
    std::vector<std::string> result;
    result.reserve(branches_.size());
    for (const auto& entry : branches_)
        result.push_back(entry.first);
    std::sort(result.begin(), result.end());
    return result;
}

const DeterministicTimeMachineFrame* UniversalDeterministicTimeMachine::FindFrame(unsigned frame) const
{
    const auto branchIt = branches_.find(currentBranch_);
    return branchIt != branches_.end() ? FindFrame(branchIt->second, frame) : nullptr;
}

const DeterministicTimeMachineFrame* UniversalDeterministicTimeMachine::FindFrame(const std::string& branch,
    unsigned frame) const
{
    const auto branchIt = branches_.find(branch);
    return branchIt != branches_.end() ? FindFrame(branchIt->second, frame) : nullptr;
}

const std::vector<DeterministicTimeMachineFrame>& UniversalDeterministicTimeMachine::GetFrames() const
{
    static const std::vector<DeterministicTimeMachineFrame> empty;
    const auto branchIt = branches_.find(currentBranch_);
    return branchIt != branches_.end() ? branchIt->second.frames : empty;
}

DeterministicFrameDifference UniversalDeterministicTimeMachine::CompareFrames(unsigned fromFrame, unsigned toFrame) const
{
    return Compare(FindFrame(fromFrame), FindFrame(toFrame));
}

DeterministicFrameDifference UniversalDeterministicTimeMachine::CompareFrames(const std::string& leftBranch,
    unsigned leftFrame, const std::string& rightBranch, unsigned rightFrame) const
{
    return Compare(FindFrame(leftBranch, leftFrame), FindFrame(rightBranch, rightFrame));
}

bool UniversalDeterministicTimeMachine::FindFirstDivergence(const std::string& leftBranch,
    const std::string& rightBranch, unsigned& frame, DeterministicFrameDifference& difference) const
{
    const auto leftIt = branches_.find(leftBranch);
    const auto rightIt = branches_.find(rightBranch);
    if (leftIt == branches_.end() || rightIt == branches_.end())
        return false;

    std::vector<unsigned> frames;
    for (const DeterministicTimeMachineFrame& item : leftIt->second.frames)
        frames.push_back(item.frame);
    for (const DeterministicTimeMachineFrame& item : rightIt->second.frames)
        frames.push_back(item.frame);
    std::sort(frames.begin(), frames.end());
    frames.erase(std::unique(frames.begin(), frames.end()), frames.end());

    for (const unsigned candidate : frames)
    {
        const DeterministicTimeMachineFrame* left = FindFrame(leftBranch, candidate);
        const DeterministicTimeMachineFrame* right = FindFrame(rightBranch, candidate);
        if (!left || !right)
        {
            difference = {};
            difference.valid = true;
            difference.fromFrame = candidate;
            difference.toFrame = candidate;
            difference.fromDigest = left ? left->digest : 0;
            difference.toDigest = right ? right->digest : 0;
            const DeterministicTimeMachineFrame* available = left ? left : right;
            for (const auto& entry : available->state)
                difference.changedKeys.emplace_back(entry.first.c_str());
            std::sort(difference.changedKeys.begin(), difference.changedKeys.end());
            frame = candidate;
            return true;
        }

        difference = Compare(left, right);
        if (difference.fromDigest != difference.toDigest)
        {
            frame = candidate;
            return true;
        }
    }
    return false;
}

unsigned long long UniversalDeterministicTimeMachine::ComputeDigest() const
{
    return ComputeBranchDigest(currentBranch_);
}

unsigned long long UniversalDeterministicTimeMachine::ComputeBranchDigest(const std::string& branch) const
{
    const auto branchIt = branches_.find(branch);
    if (branchIt == branches_.end())
        return 0;

    std::uint64_t hash = FnvOffset;
    for (const unsigned char byte : branch)
        HashByte(hash, byte);
    HashValue(hash, static_cast<std::uint64_t>(branchIt->second.frames.size()));
    for (const DeterministicTimeMachineFrame& frame : branchIt->second.frames)
    {
        HashValue(hash, frame.frame);
        HashValue(hash, frame.generation);
        HashValue(hash, static_cast<unsigned>(frame.domain));
        HashValue(hash, frame.digest);
        for (const unsigned char byte : frame.label)
            HashByte(hash, byte);
    }
    return hash;
}

void UniversalDeterministicTimeMachine::Clear()
{
    branches_.clear();
    state_.clear();
    currentFrame_ = 0;
    generation_ = 1;
    currentBranch_ = "main";
}

unsigned long long UniversalDeterministicTimeMachine::ComputeStateDigest(const StringVariantMap& state)
{
    std::vector<ea::string> keys;
    keys.reserve(state.size());
    for (const auto& entry : state)
        keys.push_back(entry.first);
    std::sort(keys.begin(), keys.end());

    std::uint64_t hash = FnvOffset;
    for (const ea::string& key : keys)
    {
        HashString(hash, key);
        const auto iterator = state.find(key);
        if (iterator != state.end())
        {
            HashByte(hash, static_cast<unsigned char>(iterator->second.GetType()));
            HashString(hash, iterator->second.ToString());
        }
    }
    return hash;
}

DeterministicFrameDifference UniversalDeterministicTimeMachine::Compare(
    const DeterministicTimeMachineFrame* left, const DeterministicTimeMachineFrame* right)
{
    DeterministicFrameDifference difference;
    if (!left || !right)
        return difference;

    difference.valid = true;
    difference.fromFrame = left->frame;
    difference.toFrame = right->frame;
    difference.fromDigest = left->digest;
    difference.toDigest = right->digest;

    std::vector<ea::string> keys;
    keys.reserve(left->state.size() + right->state.size());
    for (const auto& entry : left->state)
        keys.push_back(entry.first);
    for (const auto& entry : right->state)
    {
        if (std::find(keys.begin(), keys.end(), entry.first) == keys.end())
            keys.push_back(entry.first);
    }
    std::sort(keys.begin(), keys.end());
    for (const ea::string& key : keys)
    {
        const auto leftIt = left->state.find(key);
        const auto rightIt = right->state.find(key);
        const bool changed = leftIt == left->state.end() || rightIt == right->state.end()
            || leftIt->second.GetType() != rightIt->second.GetType()
            || leftIt->second.ToString() != rightIt->second.ToString();
        if (changed)
            difference.changedKeys.emplace_back(key.c_str());
    }
    return difference;
}

const DeterministicTimeMachineFrame* UniversalDeterministicTimeMachine::FindFrame(const Branch& branch,
    unsigned frame)
{
    for (const DeterministicTimeMachineFrame& item : branch.frames)
    {
        if (item.frame == frame)
            return &item;
    }
    return nullptr;
}

void UniversalDeterministicTimeMachine::TrimBranch(Branch& branch)
{
    if (branch.frames.size() > capacity_)
        branch.frames.erase(branch.frames.begin(), branch.frames.end() - capacity_);
}

void UniversalDeterministicTimeMachine::SetError(std::string* error, const std::string& message) const
{
    if (error)
        *error = message;
}

} // namespace Urho3D
