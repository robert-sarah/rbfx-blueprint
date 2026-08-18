// Copyright (c) 2026 the rbfx-blueprint project.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <Urho3D/Urho3D.h>
#include <Urho3D/WorldFabric/WorldFabric.h>

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Urho3D
{

enum class CausalEvidenceKind
{
    GraphChange,
    Build,
    Runtime,
    Profiler,
    Diagnostic,
    Snapshot
};

/// A deterministic piece of evidence explaining a World Fabric state.
struct URHO3D_API CausalEvidence
{
    unsigned long long sequence{};
    CausalEvidenceKind kind{CausalEvidenceKind::Diagnostic};
    WorldFabricId node{InvalidWorldFabricId};
    WorldFabricId relatedNode{InvalidWorldFabricId};
    unsigned frame{};
    unsigned long long digest{};
    unsigned long long causedBy{};
    double confidence{1.0};
    std::string source;
    std::string message;
};

/// Result of tracing the known causal history of a semantic node.
struct URHO3D_API CausalAnalysis
{
    bool found{};
    WorldFabricId target{InvalidWorldFabricId};
    unsigned long long digest{};
    std::string summary;
    std::vector<CausalEvidence> chain;
    std::vector<WorldFabricId> impactedNodes;
};

/// Records deterministic production/runtime evidence and explains likely causes.
class URHO3D_API CausalWorldFabricDebugger
{
public:
    explicit CausalWorldFabricDebugger(WorldFabricGraph* graph = nullptr);
    ~CausalWorldFabricDebugger();

    CausalWorldFabricDebugger(const CausalWorldFabricDebugger&) = delete;
    CausalWorldFabricDebugger& operator=(const CausalWorldFabricDebugger&) = delete;

    /// Attach to a graph and record its structural events automatically.
    bool Attach(WorldFabricGraph* graph, std::string* error = nullptr);
    void Detach();
    WorldFabricGraph* GetGraph() const { return graph_; }

    /// Record an evidence item and return its stable sequence number.
    unsigned long long Record(CausalEvidence evidence);
    unsigned long long RecordBuild(WorldFabricId node, const std::string& source,
        const std::string& message, unsigned long long digest = 0, unsigned frame = 0,
        unsigned long long causedBy = 0, double confidence = 1.0);
    unsigned long long RecordRuntime(WorldFabricId node, const std::string& source,
        const std::string& message, unsigned frame = 0, unsigned long long digest = 0,
        unsigned long long causedBy = 0, double confidence = 1.0);
    unsigned long long RecordDiagnostic(WorldFabricId node, const std::string& source,
        const std::string& message, unsigned frame = 0, unsigned long long causedBy = 0,
        double confidence = 1.0);

    /// Analyze the latest evidence for a node and follow explicit/semantic causes.
    CausalAnalysis Analyze(WorldFabricId target, unsigned maxDepth = 64) const;
    /// Find the latest evidence for a node, if one exists.
    bool FindLatest(WorldFabricId node, CausalEvidence& evidence) const;
    /// Return all recorded evidence in insertion/sequence order.
    const std::vector<CausalEvidence>& GetEvidence() const { return evidence_; }
    /// Compute a stable digest of the recorded causal history.
    unsigned long long ComputeDigest() const;
    /// Clear evidence while preserving the graph attachment.
    void Clear();

private:
    static unsigned long long HashText(const std::string& text);
    static const CausalEvidence* FindBySequence(const std::vector<CausalEvidence>& evidence,
        unsigned long long sequence);
    void RecordGraphEvent(const WorldFabricEvent& event);
    void SetError(std::string* error, const std::string& message) const;

    WorldFabricGraph* graph_{};
    unsigned graphSubscriptionId_{};
    unsigned long long nextSequence_{1};
    std::vector<CausalEvidence> evidence_;
    std::unordered_map<WorldFabricId, unsigned long long> latestByNode_;
};

} // namespace Urho3D
