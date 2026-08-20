// SPDX-License-Identifier: MIT

#include "RollbackManager.h"

#include "SnapshotBuffer.h"

#include <algorithm>

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
}

void RollbackManager::Clear()
{
    inputs_.clear();
    states_.clear();
    lastDiagnostics_ = {};
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
    }

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
        return false;
    }

    return Reconcile(checkpoint.frame, checkpoint.state, digest, simulator, correctedState);
}

} // namespace Urho3D
