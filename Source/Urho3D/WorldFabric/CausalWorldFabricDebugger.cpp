// Copyright (c) 2026 the rbfx-blueprint project.
//
// SPDX-License-Identifier: MIT
//

#include "CausalWorldFabricDebugger.h"

#include <Urho3D/Core/StringUtils.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace Urho3D
{

namespace
{

constexpr std::uint64_t FnvOffset = 1469598103934665603ull;
constexpr std::uint64_t FnvPrime = 1099511628211ull;

void MixByte(std::uint64_t& hash, unsigned char value)
{
    hash ^= value;
    hash *= FnvPrime;
}

template <class T> void MixValue(std::uint64_t& hash, T value)
{
    for (unsigned i = 0; i < sizeof(T); ++i)
        MixByte(hash, static_cast<unsigned char>((static_cast<std::uint64_t>(value) >> (i * 8)) & 0xff));
}

void MixText(std::uint64_t& hash, const std::string& text)
{
    MixValue(hash, static_cast<std::uint64_t>(text.size()));
    for (const unsigned char byte : text)
        MixByte(hash, byte);
}

const char* EventName(WorldFabricEventType type)
{
    switch (type)
    {
    case WorldFabricEventType::NodeAdded: return "NodeAdded";
    case WorldFabricEventType::NodeRemoved: return "NodeRemoved";
    case WorldFabricEventType::DependencyAdded: return "DependencyAdded";
    case WorldFabricEventType::DependencyRemoved: return "DependencyRemoved";
    case WorldFabricEventType::GraphReset: return "GraphReset";
    default: return "Unknown";
    }
}

} // namespace

CausalWorldFabricDebugger::CausalWorldFabricDebugger(WorldFabricGraph* graph)
{
    if (graph)
        Attach(graph);
}

CausalWorldFabricDebugger::~CausalWorldFabricDebugger()
{
    Detach();
}

bool CausalWorldFabricDebugger::Attach(WorldFabricGraph* graph, std::string* error)
{
    if (graph_ == graph && graphSubscriptionId_ != 0)
        return true;

    Detach();
    graph_ = graph;
    if (!graph_)
        return true;

    graphSubscriptionId_ = graph_->Subscribe([this](const WorldFabricEvent& event) { RecordGraphEvent(event); });
    if (graphSubscriptionId_ == 0)
    {
        graph_ = nullptr;
        SetError(error, "World Fabric graph rejected the causal debugger subscription.");
        return false;
    }
    return true;
}

void CausalWorldFabricDebugger::Detach()
{
    if (graph_ && graphSubscriptionId_ != 0)
        graph_->Unsubscribe(graphSubscriptionId_);
    graphSubscriptionId_ = 0;
    graph_ = nullptr;
}

unsigned long long CausalWorldFabricDebugger::Record(CausalEvidence evidence)
{
    evidence.sequence = nextSequence_++;
    if (!std::isfinite(evidence.confidence))
        evidence.confidence = 0.0;
    evidence.confidence = std::max(0.0, std::min(1.0, evidence.confidence));
    evidence_.push_back(std::move(evidence));
    if (evidence_.back().node != InvalidWorldFabricId)
        latestByNode_[evidence_.back().node] = evidence_.back().sequence;
    return evidence_.back().sequence;
}

unsigned long long CausalWorldFabricDebugger::RecordBuild(WorldFabricId node, const std::string& source,
    const std::string& message, unsigned long long digest, unsigned frame, unsigned long long causedBy,
    double confidence)
{
    CausalEvidence evidence;
    evidence.kind = CausalEvidenceKind::Build;
    evidence.node = node;
    evidence.frame = frame;
    evidence.digest = digest;
    evidence.causedBy = causedBy;
    evidence.confidence = confidence;
    evidence.source = source;
    evidence.message = message;
    return Record(std::move(evidence));
}

unsigned long long CausalWorldFabricDebugger::RecordRuntime(WorldFabricId node, const std::string& source,
    const std::string& message, unsigned frame, unsigned long long digest, unsigned long long causedBy,
    double confidence)
{
    CausalEvidence evidence;
    evidence.kind = CausalEvidenceKind::Runtime;
    evidence.node = node;
    evidence.frame = frame;
    evidence.digest = digest;
    evidence.causedBy = causedBy;
    evidence.confidence = confidence;
    evidence.source = source;
    evidence.message = message;
    return Record(std::move(evidence));
}

unsigned long long CausalWorldFabricDebugger::RecordDiagnostic(WorldFabricId node, const std::string& source,
    const std::string& message, unsigned frame, unsigned long long causedBy, double confidence)
{
    CausalEvidence evidence;
    evidence.kind = CausalEvidenceKind::Diagnostic;
    evidence.node = node;
    evidence.frame = frame;
    evidence.causedBy = causedBy;
    evidence.confidence = confidence;
    evidence.source = source;
    evidence.message = message;
    return Record(std::move(evidence));
}

CausalAnalysis CausalWorldFabricDebugger::Analyze(WorldFabricId target, unsigned maxDepth) const
{
    CausalAnalysis analysis;
    analysis.target = target;
    if (target == InvalidWorldFabricId || maxDepth == 0)
    {
        analysis.summary = "No valid World Fabric target was supplied.";
        return analysis;
    }

    CausalEvidence latest;
    if (!FindLatest(target, latest))
    {
        analysis.summary = "No causal evidence is recorded for the selected World Fabric node.";
    }
    else
    {
        analysis.found = true;
        unsigned long long sequence = latest.sequence;
        std::unordered_set<unsigned long long> visited;
        while (sequence != 0 && analysis.chain.size() < maxDepth && visited.insert(sequence).second)
        {
            const CausalEvidence* evidence = FindBySequence(evidence_, sequence);
            if (!evidence)
                break;
            analysis.chain.push_back(*evidence);

            if (evidence->causedBy != 0)
            {
                sequence = evidence->causedBy;
                continue;
            }
            if (evidence->relatedNode != InvalidWorldFabricId)
            {
                CausalEvidence related;
                sequence = FindLatest(evidence->relatedNode, related) ? related.sequence : 0;
                continue;
            }
            sequence = 0;
        }
        std::reverse(analysis.chain.begin(), analysis.chain.end());
        if (!analysis.chain.empty())
        {
            const ea::string summary = Format("Causal chain contains {} evidence item(s); root: {}",
                analysis.chain.size(), analysis.chain.front().message.c_str());
            analysis.summary = summary.c_str();
        }
    }

    if (graph_)
    {
        std::vector<WorldFabricId> pending{target};
        std::unordered_set<WorldFabricId> visited;
        while (!pending.empty())
        {
            const WorldFabricId node = pending.back();
            pending.pop_back();
            if (node == InvalidWorldFabricId || !visited.insert(node).second)
                continue;
            analysis.impactedNodes.push_back(node);
            for (const WorldFabricDependency& dependent : graph_->GetDependents(node))
                pending.push_back(dependent.node);
        }
        std::sort(analysis.impactedNodes.begin(), analysis.impactedNodes.end());
    }
    analysis.digest = ComputeDigest();
    return analysis;
}

bool CausalWorldFabricDebugger::FindLatest(WorldFabricId node, CausalEvidence& evidence) const
{
    const auto iterator = latestByNode_.find(node);
    if (iterator == latestByNode_.end())
        return false;
    const CausalEvidence* latest = FindBySequence(evidence_, iterator->second);
    if (!latest)
        return false;
    evidence = *latest;
    return true;
}

unsigned long long CausalWorldFabricDebugger::ComputeDigest() const
{
    std::uint64_t hash = FnvOffset;
    MixValue(hash, static_cast<std::uint64_t>(evidence_.size()));
    for (const CausalEvidence& evidence : evidence_)
    {
        MixValue(hash, evidence.sequence);
        MixValue(hash, static_cast<unsigned>(evidence.kind));
        MixValue(hash, evidence.node);
        MixValue(hash, evidence.relatedNode);
        MixValue(hash, evidence.frame);
        MixValue(hash, evidence.digest);
        MixValue(hash, evidence.causedBy);
        MixValue(hash, static_cast<std::uint64_t>(evidence.confidence * 1000000.0));
        MixText(hash, evidence.source);
        MixText(hash, evidence.message);
    }
    return hash;
}

void CausalWorldFabricDebugger::Clear()
{
    evidence_.clear();
    latestByNode_.clear();
    nextSequence_ = 1;
}

unsigned long long CausalWorldFabricDebugger::HashText(const std::string& text)
{
    std::uint64_t hash = FnvOffset;
    MixText(hash, text);
    return hash;
}

const CausalEvidence* CausalWorldFabricDebugger::FindBySequence(const std::vector<CausalEvidence>& evidence,
    unsigned long long sequence)
{
    for (const CausalEvidence& item : evidence)
    {
        if (item.sequence == sequence)
            return &item;
    }
    return nullptr;
}

void CausalWorldFabricDebugger::RecordGraphEvent(const WorldFabricEvent& event)
{
    CausalEvidence evidence;
    evidence.kind = CausalEvidenceKind::GraphChange;
    evidence.node = event.node;
    evidence.relatedNode = event.dependency;
    evidence.source = "WorldFabricGraph";
    evidence.message = EventName(event.type);
    evidence.digest = graph_ ? graph_->ComputeDigest() : 0;
    if (event.dependency != InvalidWorldFabricId)
    {
        CausalEvidence dependencyEvidence;
        if (FindLatest(event.dependency, dependencyEvidence))
            evidence.causedBy = dependencyEvidence.sequence;
    }
    Record(std::move(evidence));
}

void CausalWorldFabricDebugger::SetError(std::string* error, const std::string& message) const
{
    if (error)
        *error = message;
}

} // namespace Urho3D
