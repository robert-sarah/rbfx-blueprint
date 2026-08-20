// Copyright (c) 2026 the rbfx-blueprint project.
//
// SPDX-License-Identifier: MIT
//

#include "UniversalDeterministicTimeMachine.h"

#include <Urho3D/Resource/JSONFile.h>

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <limits>
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

std::string UInt64ToString(std::uint64_t value)
{
    return std::to_string(value);
}

bool ParseUInt64(const JSONValue& value, std::uint64_t& result)
{
    if (!value.IsString() || value.GetString().empty())
        return false;

    errno = 0;
    char* end = nullptr;
    const char* begin = value.GetCString();
    const unsigned long long parsed = std::strtoull(begin, &end, 10);
    if (errno == ERANGE || end == begin || *end != '\0')
        return false;
    result = static_cast<std::uint64_t>(parsed);
    return true;
}

bool IsStrictlyAfter(unsigned frame, unsigned previous)
{
    return frame > previous;
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

JSONValue UniversalDeterministicTimeMachine::ToJSON() const
{
    JSONValue root(JSON_OBJECT);
    root.Set("version", 1u);
    root.Set("fixedDelta", fixedDelta_);
    root.Set("capacity", capacity_);
    root.Set("currentFrame", currentFrame_);
    root.Set("generation", generation_);
    root.Set("currentBranch", currentBranch_.c_str());

    JSONValue currentState(JSON_OBJECT);
    currentState.SetStringVariantMap(state_);
    root.Set("state", ea::move(currentState));

    JSONValue branches(JSON_ARRAY);
    for (const std::string& branchName : GetBranches())
    {
        const Branch& branch = branches_.at(branchName);
        JSONValue branchValue(JSON_OBJECT);
        branchValue.Set("name", branchName.c_str());

        JSONValue frames(JSON_ARRAY);
        for (const DeterministicTimeMachineFrame& frame : branch.frames)
        {
            JSONValue frameValue(JSON_OBJECT);
            frameValue.Set("frame", frame.frame);
            frameValue.Set("generation", frame.generation);
            frameValue.Set("domain", static_cast<unsigned>(frame.domain));
            frameValue.Set("digest", UInt64ToString(frame.digest).c_str());
            frameValue.Set("label", frame.label.c_str());

            JSONValue frameState(JSON_OBJECT);
            frameState.SetStringVariantMap(frame.state);
            frameValue.Set("state", ea::move(frameState));

            JSONValue frameInput(JSON_OBJECT);
            frameInput.SetStringVariantMap(frame.input);
            frameValue.Set("input", ea::move(frameInput));
            frames.Push(ea::move(frameValue));
        }
        branchValue.Set("frames", ea::move(frames));
        branches.Push(ea::move(branchValue));
    }
    root.Set("branches", ea::move(branches));
    return root;
}

bool UniversalDeterministicTimeMachine::FromJSON(const JSONValue& value, std::string* error)
{
    auto fail = [this, error](const std::string& message)
    {
        SetError(error, message);
        return false;
    };

    if (!value.IsObject())
        return fail("The deterministic replay root must be a JSON object.");
    if (!value.Contains("version") || value["version"].GetUInt() != 1)
        return fail("Unsupported deterministic replay version.");
    if (!value.Contains("fixedDelta") || !value["fixedDelta"].IsNumber()
        || value["fixedDelta"].GetFloat() <= 0.0f)
        return fail("The deterministic replay fixed delta must be positive.");
    if (!value.Contains("capacity") || value["capacity"].GetUInt() < 2)
        return fail("The deterministic replay capacity must be at least two frames.");
    if (!value.Contains("currentBranch") || !value["currentBranch"].IsString()
        || value["currentBranch"].GetString().empty())
        return fail("The deterministic replay current branch is missing.");
    if (!value.Contains("state") || !value["state"].IsObject())
        return fail("The deterministic replay current state must be an object.");
    if (!value.Contains("branches") || !value["branches"].IsArray())
        return fail("The deterministic replay branches must be an array.");

    const unsigned currentFrame = value.Contains("currentFrame") ? value["currentFrame"].GetUInt() : 0;
    const unsigned generation = value.Contains("generation") ? value["generation"].GetUInt() : 1;
    const std::string currentBranch = value["currentBranch"].GetString().c_str();
    const float fixedDelta = value["fixedDelta"].GetFloat();
    const unsigned capacity = value["capacity"].GetUInt();
    const StringVariantMap state = value["state"].GetStringVariantMap();

    std::unordered_map<std::string, Branch> parsedBranches;
    for (const JSONValue& branchValue : value["branches"].GetArray())
    {
        if (!branchValue.IsObject() || !branchValue.Contains("name") || !branchValue["name"].IsString()
            || !branchValue.Contains("frames") || !branchValue["frames"].IsArray())
            return fail("Each deterministic replay branch requires a name and frames array.");

        const std::string branchName = branchValue["name"].GetString().c_str();
        if (branchName.empty() || parsedBranches.find(branchName) != parsedBranches.end())
            return fail("Deterministic replay branch names must be non-empty and unique.");

        Branch branch;
        unsigned previousFrame = 0;
        bool hasPreviousFrame = false;
        for (const JSONValue& frameValue : branchValue["frames"].GetArray())
        {
            if (!frameValue.IsObject() || !frameValue.Contains("frame") || !frameValue.Contains("generation")
                || !frameValue.Contains("domain") || !frameValue.Contains("digest")
                || !frameValue.Contains("state") || !frameValue.Contains("input")
                || !frameValue["state"].IsObject() || !frameValue["input"].IsObject())
                return fail("Each deterministic replay frame is missing required fields.");

            const unsigned frameNumber = frameValue["frame"].GetUInt();
            if (hasPreviousFrame && !IsStrictlyAfter(frameNumber, previousFrame))
                return fail("Deterministic replay frames must be strictly ordered.");
            const unsigned domain = frameValue["domain"].GetUInt();
            if (domain > static_cast<unsigned>(DeterministicTimeMachineDomain::Custom))
                return fail("Deterministic replay frame domain is invalid.");

            std::uint64_t digest = 0;
            if (!ParseUInt64(frameValue["digest"], digest))
                return fail("Deterministic replay frame digest must be an unsigned decimal string.");

            DeterministicTimeMachineFrame frame;
            frame.frame = frameNumber;
            frame.generation = frameValue["generation"].GetUInt();
            frame.domain = static_cast<DeterministicTimeMachineDomain>(domain);
            frame.state = frameValue["state"].GetStringVariantMap();
            frame.input = frameValue["input"].GetStringVariantMap();
            frame.digest = digest;
            frame.label = frameValue.Contains("label") && frameValue["label"].IsString()
                ? frameValue["label"].GetString().c_str() : std::string();
            if (ComputeStateDigest(frame.state) != frame.digest)
                return fail("Deterministic replay frame state digest does not match its state.");

            branch.frames.push_back(std::move(frame));
            previousFrame = frameNumber;
            hasPreviousFrame = true;
        }

        if (branch.frames.empty())
            return fail("Deterministic replay branches cannot be empty.");
        parsedBranches.emplace(branchName, std::move(branch));
    }

    if (parsedBranches.empty())
    {
        if (currentFrame != 0 || !state.empty())
            return fail("An empty deterministic replay can only have an empty frame-zero state.");
    }
    else
    {
        const auto currentBranchIt = parsedBranches.find(currentBranch);
        if (currentBranchIt == parsedBranches.end())
            return fail("The deterministic replay current branch does not exist.");
        const DeterministicTimeMachineFrame* currentSnapshot = FindFrame(currentBranchIt->second, currentFrame);
        if (!currentSnapshot || ComputeStateDigest(state) != currentSnapshot->digest)
            return fail("The deterministic replay current state does not match its current frame.");
    }

    fixedDelta_ = fixedDelta;
    capacity_ = capacity;
    currentFrame_ = currentFrame;
    generation_ = generation;
    currentBranch_ = currentBranch;
    state_ = state;
    branches_ = std::move(parsedBranches);
    return true;
}

std::string UniversalDeterministicTimeMachine::ExportReplay() const
{
    auto serialize = [this](Context* context)
    {
        JSONFile file(context);
        file.GetRoot() = ToJSON();
        const ea::string text = file.ToString("  ");
        return std::string(text.c_str(), text.size());
    };

    if (Context* context = Context::GetInstance())
        return serialize(context);

    Context temporaryContext;
    return serialize(&temporaryContext);
}

bool UniversalDeterministicTimeMachine::ImportReplay(const std::string& json, std::string* error)
{
    JSONValue root;
    const ea::string source(json.c_str());
    if (!JSONFile::ParseJSON(source, root, false))
    {
        SetError(error, "The deterministic replay is not valid JSON.");
        return false;
    }
    return FromJSON(root, error);
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
