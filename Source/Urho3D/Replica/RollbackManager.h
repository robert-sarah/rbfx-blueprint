// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <EASTL/functional.h>
#include <EASTL/vector.h>

#include <Urho3D/Core/Variant.h>
#include <Urho3D/Replica/NetworkId.h>
#include <Urho3D/Replica/SnapshotBuffer.h>

namespace Urho3D
{

/// Local input recorded against a simulation frame.
struct URHO3D_API RollbackInput
{
    NetworkFrame frame{NetworkFrame::Min};
    StringVariantMap values;
};

/// Evidence produced by the last authoritative reconciliation.
struct URHO3D_API RollbackDiagnostics
{
    NetworkFrame authoritativeFrame{NetworkFrame::Min};
    unsigned long long authoritativeDigest{};
    unsigned long long predictedDigest{};
    unsigned long long correctedDigest{};
    unsigned replayedInputs{};
    unsigned predictedInputs{};
    unsigned rejectedInputs{};
    bool diverged{};
};

/// Authoritative checkpoint that can be transported through any network layer.
struct URHO3D_API RollbackResynchronization
{
    NetworkFrame frame{NetworkFrame::Min};
    unsigned long long digest{};
    StringVariantMap state;
};

using RollbackSimulator = ea::function<bool(const StringVariantMap&, StringVariantMap&)>;

/// Bounded client-prediction history and authoritative-state reconciliation.
///
/// The manager is transport-independent: a Network or Replica layer supplies the
/// authoritative checkpoint and simulator callback, while this class guarantees
/// ordered input replay, bounded retention and divergence evidence.
class URHO3D_API RollbackManager
{
public:
    explicit RollbackManager(unsigned capacity = 64);

    void SetCapacity(unsigned capacity);
    unsigned GetCapacity() const { return capacity_; }
    void Clear();

    void RecordInput(const RollbackInput& input);
    void SaveState(NetworkFrame frame, const StringVariantMap& state);
    const StringVariantMap* FindState(NetworkFrame frame) const;

    /// Replace state at authoritativeFrame and replay later inputs through simulator.
    bool Reconcile(NetworkFrame authoritativeFrame, const StringVariantMap& authoritativeState,
        const RollbackSimulator& simulator, StringVariantMap& correctedState);

    /// Reconcile with an externally supplied authoritative digest for desync evidence.
    bool Reconcile(NetworkFrame authoritativeFrame, const StringVariantMap& authoritativeState,
        unsigned long long authoritativeDigest, const RollbackSimulator& simulator,
        StringVariantMap& correctedState);

    /// Apply a transport-independent resynchronization checkpoint and replay retained inputs.
    bool ApplyResynchronization(const RollbackResynchronization& checkpoint,
        const RollbackSimulator& simulator, StringVariantMap& correctedState);

    const RollbackDiagnostics& GetLastDiagnostics() const { return lastDiagnostics_; }
    static unsigned long long ComputeStateDigest(const StringVariantMap& state);

    const ea::vector<RollbackInput>& GetInputs() const { return inputs_; }
    const ea::vector<NetworkSnapshot>& GetStates() const { return states_; }

private:
    unsigned capacity_{64};
    ea::vector<RollbackInput> inputs_;
    ea::vector<NetworkSnapshot> states_;
    RollbackDiagnostics lastDiagnostics_;
};

} // namespace Urho3D
