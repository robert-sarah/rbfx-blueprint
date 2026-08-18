// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <Urho3D/Math/Vector3.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace Urho3D
{

class WorldPartition;

struct OpenWorldCellWork
{
    std::string cellId;
    bool load{true};
    float distanceSquared{0.0f};
    unsigned estimatedMemoryMb{0};
    unsigned priority{0};
};

/// Deterministic orchestration layer around WorldPartition.
/// It schedules work; the caller remains responsible for performing scene I/O.
class OpenWorldStreamingScheduler
{
public:
    void SetFrameBudget(unsigned operationsPerFrame) { operationsPerFrame_ = operationsPerFrame ? operationsPerFrame : 1; }
    unsigned GetFrameBudget() const { return operationsPerFrame_; }
    void SetMemoryBudgetMb(unsigned memoryBudgetMb) { memoryBudgetMb_ = memoryBudgetMb; }
    unsigned GetMemoryBudgetMb() const { return memoryBudgetMb_; }

    unsigned Update(WorldPartition& partition, const Vector3& observerPosition);
    bool PopNext(OpenWorldCellWork& work);
    void Complete(const std::string& cellId, bool success, unsigned residentMemoryMb = 0);
    void Clear();
    unsigned GetPendingCount() const { return pending_.size(); }
    unsigned GetInFlightCount() const { return inFlight_.size(); }
    unsigned GetResidentMemoryMb() const { return residentMemoryMb_; }
    const std::string& GetLastError() const { return lastError_; }

private:
    void SortPending();

    std::vector<OpenWorldCellWork> pending_;
    std::unordered_map<std::string, OpenWorldCellWork> inFlight_;
    WorldPartition* activePartition_{nullptr};
    unsigned operationsPerFrame_{4};
    unsigned memoryBudgetMb_{4096};
    unsigned residentMemoryMb_{0};
    std::string lastError_;
};

struct OpenWorldOcclusionProxy
{
    std::string id;
    Vector3 center{Vector3::ZERO};
    Vector3 halfExtents{Vector3::ONE};
    bool enabled{true};
};

/// Conservative deterministic occlusion policy for cell and proxy visibility.
/// Renderer backends can replace the visibility result with hardware occlusion.
class OpenWorldOcclusionCuller
{
public:
    bool AddProxy(const OpenWorldOcclusionProxy& proxy);
    bool RemoveProxy(const std::string& id);
    void Clear();
    std::vector<std::string> QueryVisible(const Vector3& cameraPosition, const Vector3& cameraDirection,
        float maxDistance, float horizontalFovDegrees = 90.0f) const;
    const OpenWorldOcclusionProxy* GetProxy(const std::string& id) const;

private:
    std::unordered_map<std::string, OpenWorldOcclusionProxy> proxies_;
};

struct OpenWorldVegetationInstance
{
    std::string archetype;
    Vector3 position{Vector3::ZERO};
    float scale{1.0f};
    float rotationDegrees{0.0f};
    uint64_t seed{0};
};

/// Reproducible vegetation scatterer. It produces placement data; rendering remains renderer-owned.
class OpenWorldVegetationSystem
{
public:
    void SetSeed(uint64_t seed) { seed_ = seed; }
    uint64_t GetSeed() const { return seed_; }
    void SetCellSize(float cellSize) { cellSize_ = cellSize > 1.0f ? cellSize : 1.0f; }
    void SetDensity(unsigned density) { density_ = density; }
    std::vector<OpenWorldVegetationInstance> GenerateCell(const std::string& cellId, const Vector3& origin) const;

private:
    static uint64_t Hash(uint64_t value);
    uint64_t seed_{0xA11CE5EEDULL};
    float cellSize_{64.0f};
    unsigned density_{32};
};

enum class OpenWorldNavigationState
{
    Unbuilt,
    Building,
    Ready,
    Stale,
    Failed,
};

struct OpenWorldCellNavigation
{
    std::string cellId;
    OpenWorldNavigationState state{OpenWorldNavigationState::Unbuilt};
    unsigned revision{0};
    unsigned polygonCount{0};
};

/// Tracks navigation readiness independently per streaming cell.
class OpenWorldCellNavigationService
{
public:
    bool RegisterCell(const std::string& cellId);
    bool SetState(const std::string& cellId, OpenWorldNavigationState state, unsigned polygonCount = 0);
    bool MarkStale(const std::string& cellId);
    const OpenWorldCellNavigation* GetCell(const std::string& cellId) const;
    std::vector<std::string> GetReadyCells() const;
    void Clear();

private:
    std::unordered_map<std::string, OpenWorldCellNavigation> cells_;
};

enum class OpenWorldCharacterLOD
{
    Near,
    Mid,
    Far,
    Culled,
};

struct OpenWorldCharacterLODResult
{
    OpenWorldCharacterLOD level{OpenWorldCharacterLOD::Near};
    float normalizedDistance{0.0f};
    bool castShadows{true};
};

/// Distance-based character LOD policy, ready to be connected to renderer and animation budgets.
class OpenWorldCharacterLODPolicy
{
public:
    void SetDistances(float nearDistance, float midDistance, float farDistance);
    OpenWorldCharacterLODResult Evaluate(float distance, bool isInFrustum = true) const;
    float GetNearDistance() const { return nearDistance_; }
    float GetMidDistance() const { return midDistance_; }
    float GetFarDistance() const { return farDistance_; }

private:
    float nearDistance_{15.0f};
    float midDistance_{50.0f};
    float farDistance_{120.0f};
};

} // namespace Urho3D
