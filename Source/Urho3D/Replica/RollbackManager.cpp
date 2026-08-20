// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#include "RollbackManager.h"

#include "SnapshotBuffer.h"

#include <algorithm>
#include <limits>

namespace Urho3D
{

namespace
{

void HashByte(unsigned long long& hash, unsigned char value)
{
    hash ^= value;
    hash *= 1099511628211ull;
}

void HashString(unsigned long long& hash, const ea::string& value)
{
    for (unsigned char byte : value)
        HashByte(hash, byte);
    HashByte(hash, 0);
}

bool IsBefore(NetworkFrame left, NetworkFrame right)
{
    return static_cast<long long>(left) < static_cast<long long>(right);
}

} // namespace

RollbackManager::RollbackManager(unsigned capacity)
{
    SetCapacity(capacity);
}

void RollbackManager::SetCapacity(unsigned capacity)
{
    capacity_ = Max(capacity, 1u);
    while (inputs_.size() > capacity_)
        inputs_.erase(inputs_.begin());
    while (states_.size() > capacity_)
        states_.erase(states_.begin());
    while (digests_.size() > capacity_)
        digests_.erase(digests_.begin());
}

void RollbackManager::Clear()
{
    inputs_.clear();
    states_.clear();
    digests_.clear();
    latestAuthoritativeFrame_ = NetworkFrame::Min;
    lastDiagnostics_ = {};
}

unsigned RollbackManager::GetPredictionDepth(NetworkFrame frame) const
{
    if (latestAuthoritativeFrame_ == NetworkFrame::Min || frame <= latestAuthoritativeFrame_)
        return 0;

    const long long depth = static_cast<long long>(frame) - static_cast<long long>(latestAuthoritativeFrame_);
    if (depth >= static_cast<long long>(std::numeric_limits<unsigned>::max()))
        return std::numeric_limits<unsigned>::max();
    return static_cast<unsigned>(depth);
}

bool RollbackManager::IsPredictionAllowed(NetworkFrame frame) const
{
    return latestAuthoritativeFrame_ == NetworkFrame::Min || GetPredictionDepth(frame) <= predictionWindow_;
}

void RollbackManager::RecordInput(const RollbackInput& input)
{
    for (RollbackInput& current : inputs_)
    {
        if (current.frame == input.frame)
        {
            current = input;
            return;
        }
    }

    const auto position = std::lower_bound(inputs_.begin(), inputs_.end(), input.frame,
        [](const RollbackInput& current, NetworkFrame frame)
        {
            return IsBefore(current.frame, frame);
        });
    inputs_.insert(position, input);
    while (inputs_.size() > capacity_)
        inputs_.erase(inputs_.begin());
}

void RollbackManager::SaveState(NetworkFrame frame, const StringVariantMap& state)
{
    for (NetworkSnapshot& current : states_)
    {
        if (current.frame == frame)
        {
            current.values = state;
            SaveDigest(frame, ComputeStateDigest(state));
            return;
        }
    }

    NetworkSnapshot snapshot;
    snapshot.frame = frame;
    snapshot.time = static_cast<float>(static_cast<long long>(frame));
    snapshot.values = state;

    const auto position = std::lower_bound(states_.begin(), states_.end(), frame,
        [](const NetworkSnapshot& current, NetworkFrame value)
        {
            return IsBefore(current.frame, value);
        });
    states_.insert(position, snapshot);
    while (states_.size() > capacity_)
        states_.erase(states_.begin());
    SaveDigest(frame, ComputeStateDigest(state));
}

void RollbackManager::SaveDigest(NetworkFrame frame, unsigned long long digest)
{
    for (RollbackDigestSample& current : digests_)
    {
        if (current.frame == frame)
        {
            current.digest = digest;
            return;
        }
    }

    RollbackDigestSample sample;
    sample.frame = frame;
    sample.digest = digest;
    const auto position = std::lower_bound(digests_.begin(), digests_.end(), frame,
        [](const RollbackDigestSample& current, NetworkFrame value)
        {
            return IsBefore(current.frame, value);
        });
    digests_.insert(position, sample);
    while (digests_.size() > capacity_)
        digests_.erase(digests_.begin());
}

const StringVariantMap* RollbackManager::FindState(NetworkFrame frame) const
{
    for (const NetworkSnapshot& state : states_)
    {
        if (state.frame == frame)
            return &state.values;
    }
    return nullptr;
}

unsigned long long RollbackManager::ComputeStateDigest(const StringVariantMap& state)
{
    ea::vector<ea::string> keys;
    keys.reserve(state.size());
    for (const auto& entry : state)
        keys.push_back(entry.first);
    std::sort(keys.begin(), keys.end());

    unsigned long long hash = 1469598103934665603ull;
    for (const ea::string& key : keys)
    {
        HashString(hash, key);
        const auto it = state.find(key);
        if (it != state.end())
        {
            HashByte(hash, static_cast<unsigned char>(it->second.GetType()));
            HashString(hash, it->second.ToString());
        }
    }
    return hash;
}

bool RollbackManager::Reconcile(NetworkFrame authoritativeFrame, const StringVariantMap& authoritativeState,
    const RollbackSimulator& simulator, StringVariantMap& correctedState)
{
    return Reconcile(authoritativeFrame, authoritativeState, ComputeStateDigest(authoritativeState), simulator,
        correctedState);
}

bool RollbackManager::Reconcile(NetworkFrame authoritativeFrame, const StringVariantMap& authoritativeState,
    unsigned long long authoritativeDigest, const RollbackSimulator& simulator, StringVariantMap& correctedState)
{
    lastDiagnostics_ = {};
    lastDiagnostics_.authoritativeFrame = authoritativeFrame;
    lastDiagnostics_.authoritativeDigest = authoritativeDigest;

    const StringVariantMap* predictedState = FindState(authoritativeFrame);
    if (predictedState)
    {
        lastDiagnostics_.predictedDigest = ComputeStateDigest(*predictedState);
        lastDiagnostics_.diverged = lastDiagnostics_.predictedDigest != authoritativeDigest;
        if (lastDiagnostics_.diverged)
        {
            lastDiagnostics_.desyncDetected = true;
            lastDiagnostics_.firstDivergentFrame = authoritativeFrame;
        }
    }
    if (latestAuthoritativeFrame_ == NetworkFrame::Min || authoritativeFrame > latestAuthoritativeFrame_)
        latestAuthoritativeFrame_ = authoritativeFrame;

    for (const RollbackInput& input : inputs_)
    {
        if (input.frame > authoritativeFrame)
            ++lastDiagnostics_.predictedInputs;
    }

    correctedState = authoritativeState;
    SaveState(authoritativeFrame, correctedState);

    for (const RollbackInput& input : inputs_)
    {
        if (input.frame <= authoritativeFrame)
            continue;
        if (!simulator || !simulator(input.values, correctedState))
        {
            ++lastDiagnostics_.rejectedInputs;
            lastDiagnostics_.correctedDigest = ComputeStateDigest(correctedState);
            return false;
        }
        ++lastDiagnostics_.replayedInputs;
        SaveState(input.frame, correctedState);
    }

    lastDiagnostics_.correctedDigest = ComputeStateDigest(correctedState);
    return true;
}

const RollbackDigestSample* RollbackManager::FindDigest(NetworkFrame frame) const
{
    for (const RollbackDigestSample& sample : digests_)
    {
        if (sample.frame == frame)
            return &sample;
    }
    return nullptr;
}

bool RollbackManager::ValidateAuthoritativeDigest(NetworkFrame frame, unsigned long long authoritativeDigest)
{
    const RollbackDigestSample* predicted = FindDigest(frame);
    if (!predicted)
        return false;

    lastDiagnostics_.authoritativeFrame = frame;
    lastDiagnostics_.authoritativeDigest = authoritativeDigest;
    lastDiagnostics_.predictedDigest = predicted->digest;
    if (latestAuthoritativeFrame_ == NetworkFrame::Min || frame > latestAuthoritativeFrame_)
        latestAuthoritativeFrame_ = frame;
    ++lastDiagnostics_.comparedDigests;
    if (predicted->digest == authoritativeDigest)
        return true;

    lastDiagnostics_.diverged = true;
    lastDiagnostics_.desyncDetected = true;
    lastDiagnostics_.firstDivergentFrame = frame;
    return false;
}

bool RollbackManager::ValidateAuthoritativeDigests(
    const ea::vector<RollbackDigestSample>& authoritativeDigests)
{
    lastDiagnostics_ = {};
    for (const RollbackDigestSample& authoritative : authoritativeDigests)
    {
        if (!ValidateAuthoritativeDigest(authoritative.frame, authoritative.digest))
            return false;
    }
    return true;
}

bool RollbackManager::ApplyResynchronization(const RollbackResynchronization& checkpoint,
    const RollbackSimulator& simulator, StringVariantMap& correctedState)
{
    const unsigned long long digest = checkpoint.digest ? checkpoint.digest : ComputeStateDigest(checkpoint.state);
    if (checkpoint.digest && ComputeStateDigest(checkpoint.state) != checkpoint.digest)
    {
        lastDiagnostics_ = {};
        lastDiagnostics_.authoritativeFrame = checkpoint.frame;
        lastDiagnostics_.authoritativeDigest = checkpoint.digest;
        lastDiagnostics_.correctedDigest = ComputeStateDigest(checkpoint.state);
        lastDiagnostics_.diverged = true;
        lastDiagnostics_.desyncDetected = true;
        lastDiagnostics_.firstDivergentFrame = checkpoint.frame;
        return false;
    }

    return Reconcile(checkpoint.frame, checkpoint.state, digest, simulator, correctedState);
}

} // namespace Urho3D
