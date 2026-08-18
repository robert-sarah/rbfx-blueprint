// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#include "OpenWorldRuntime.h"

#include <Urho3D/Scene/WorldPartition.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace Urho3D
{

unsigned OpenWorldStreamingScheduler::Update(WorldPartition& partition, const Vector3& observerPosition)
{
    activePartition_ = &partition;
    partition.Update(observerPosition);

    unsigned moved = 0;
    StreamingOperation operation;
    while (moved < operationsPerFrame_ && partition.PopNextOperation(operation))
    {
        const std::string cellId{operation.cellId.c_str()};
        if (inFlight_.find(cellId) != inFlight_.end())
            continue;

        OpenWorldCellWork work;
        work.cellId = cellId;
        work.load = operation.type == StreamingOperationType::Load;
        work.distanceSquared = operation.distanceSquared;
        if (const StreamingCell* cell = partition.GetCell(operation.cellId))
            work.estimatedMemoryMb = cell->GetDescriptor().memoryCost;
        work.priority = work.load ? 100u : 10u;
        pending_.push_back(work);
        ++moved;
    }

    SortPending();
    return moved;
}

bool OpenWorldStreamingScheduler::PopNext(OpenWorldCellWork& work)
{
    if (pending_.empty())
        return false;

    work = pending_.front();
    pending_.erase(pending_.begin());
    inFlight_[work.cellId] = work;
    return true;
}

void OpenWorldStreamingScheduler::Complete(const std::string& cellId, bool success, unsigned residentMemoryMb)
{
    const auto iter = inFlight_.find(cellId);
    if (iter == inFlight_.end())
    {
        lastError_ = "Cannot complete an unknown open-world streaming operation: " + cellId;
        return;
    }

    const OpenWorldCellWork work = iter->second;
    inFlight_.erase(iter);
    if (activePartition_)
    {
        const ea::string id{cellId.c_str()};
        const ea::string error = success ? EMPTY_STRING : ea::string{"Open-world streaming operation failed"};
        activePartition_->CompleteOperation(id, success, error);
    }

    if (work.load)
    {
        if (success)
            residentMemoryMb_ += residentMemoryMb ? residentMemoryMb : work.estimatedMemoryMb;
    }
    else
    {
        const unsigned released = residentMemoryMb ? residentMemoryMb : work.estimatedMemoryMb;
        residentMemoryMb_ = released > residentMemoryMb_ ? 0 : residentMemoryMb_ - released;
    }

    if (residentMemoryMb_ > memoryBudgetMb_)
        lastError_ = "Open-world streaming memory budget exceeded";
    else
        lastError_.clear();
}

void OpenWorldStreamingScheduler::Clear()
{
    pending_.clear();
    inFlight_.clear();
    residentMemoryMb_ = 0;
    lastError_.clear();
    activePartition_ = nullptr;
}

void OpenWorldStreamingScheduler::SortPending()
{
    std::sort(pending_.begin(), pending_.end(), [](const OpenWorldCellWork& lhs, const OpenWorldCellWork& rhs)
    {
        if (lhs.priority != rhs.priority)
            return lhs.priority > rhs.priority;
        if (lhs.distanceSquared != rhs.distanceSquared)
            return lhs.distanceSquared < rhs.distanceSquared;
        return lhs.cellId < rhs.cellId;
    });
}

bool OpenWorldOcclusionCuller::AddProxy(const OpenWorldOcclusionProxy& proxy)
{
    if (proxy.id.empty() || proxy.halfExtents.x_ < 0.0f || proxy.halfExtents.y_ < 0.0f || proxy.halfExtents.z_ < 0.0f)
        return false;
    return proxies_.emplace(proxy.id, proxy).second;
}

bool OpenWorldOcclusionCuller::RemoveProxy(const std::string& id)
{
    return proxies_.erase(id) != 0;
}

void OpenWorldOcclusionCuller::Clear()
{
    proxies_.clear();
}

std::vector<std::string> OpenWorldOcclusionCuller::QueryVisible(const Vector3& cameraPosition,
    const Vector3& cameraDirection, float maxDistance, float horizontalFovDegrees) const
{
    std::vector<std::string> result;
    if (maxDistance <= 0.0f || horizontalFovDegrees <= 0.0f || horizontalFovDegrees >= 180.0f)
        return result;

    const Vector3 direction = cameraDirection.Normalized();
    const float cosineLimit = std::cos(horizontalFovDegrees * M_DEGTORAD * 0.5f);
    const float maxDistanceSquared = maxDistance * maxDistance;
    for (const auto& entry : proxies_)
    {
        const OpenWorldOcclusionProxy& proxy = entry.second;
        if (!proxy.enabled)
            continue;

        const Vector3 offset = proxy.center - cameraPosition;
        const float distanceSquared = offset.LengthSquared();
        if (distanceSquared > maxDistanceSquared)
            continue;

        const float distance = std::sqrt(distanceSquared);
        if (distance > 0.001f && direction.DotProduct(offset / distance) < cosineLimit)
            continue;
        result.push_back(proxy.id);
    }

    std::sort(result.begin(), result.end());
    return result;
}

const OpenWorldOcclusionProxy* OpenWorldOcclusionCuller::GetProxy(const std::string& id) const
{
    const auto iter = proxies_.find(id);
    return iter != proxies_.end() ? &iter->second : nullptr;
}

uint64_t OpenWorldVegetationSystem::Hash(uint64_t value)
{
    value ^= value >> 30;
    value *= 0xbf58476d1ce4e5b9ULL;
    value ^= value >> 27;
    value *= 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}

std::vector<OpenWorldVegetationInstance> OpenWorldVegetationSystem::GenerateCell(const std::string& cellId, const Vector3& origin) const
{
    std::vector<OpenWorldVegetationInstance> result;
    result.reserve(density_);
    uint64_t state = Hash(seed_ ^ std::hash<std::string>{}(cellId));
    const float step = cellSize_ / static_cast<float>(density_ ? density_ : 1);
    for (unsigned i = 0; i < density_; ++i)
    {
        state = Hash(state + i + 1);
        const float jitterX = static_cast<float>((state >> 8) & 0xffffu) / 65535.0f;
        const float jitterZ = static_cast<float>((state >> 24) & 0xffffu) / 65535.0f;
        const float scale = 0.75f + static_cast<float>((state >> 40) & 0xffffu) / 262140.0f;
        const float rotation = static_cast<float>((state >> 48) & 0xffffu) / 65535.0f * 360.0f;

        OpenWorldVegetationInstance instance;
        instance.archetype = "default";
        instance.position = origin + Vector3{(static_cast<float>(i) + jitterX) * step, 0.0f,
            (static_cast<float>((i * 7u) % (density_ ? density_ : 1)) + jitterZ) * step};
        instance.scale = scale;
        instance.rotationDegrees = rotation;
        instance.seed = state;
        result.push_back(instance);
    }
    return result;
}

bool OpenWorldCellNavigationService::RegisterCell(const std::string& cellId)
{
    if (cellId.empty())
        return false;
    OpenWorldCellNavigation cell;
    cell.cellId = cellId;
    return cells_.emplace(cellId, cell).second;
}

bool OpenWorldCellNavigationService::SetState(const std::string& cellId, OpenWorldNavigationState state, unsigned polygonCount)
{
    const auto iter = cells_.find(cellId);
    if (iter == cells_.end())
        return false;
    if (iter->second.state != state)
        ++iter->second.revision;
    iter->second.state = state;
    iter->second.polygonCount = polygonCount;
    return true;
}

bool OpenWorldCellNavigationService::MarkStale(const std::string& cellId)
{
    return SetState(cellId, OpenWorldNavigationState::Stale);
}

const OpenWorldCellNavigation* OpenWorldCellNavigationService::GetCell(const std::string& cellId) const
{
    const auto iter = cells_.find(cellId);
    return iter != cells_.end() ? &iter->second : nullptr;
}

std::vector<std::string> OpenWorldCellNavigationService::GetReadyCells() const
{
    std::vector<std::string> result;
    for (const auto& entry : cells_)
    {
        if (entry.second.state == OpenWorldNavigationState::Ready)
            result.push_back(entry.first);
    }
    std::sort(result.begin(), result.end());
    return result;
}

void OpenWorldCellNavigationService::Clear()
{
    cells_.clear();
}

void OpenWorldCharacterLODPolicy::SetDistances(float nearDistance, float midDistance, float farDistance)
{
    nearDistance_ = std::max(0.01f, nearDistance);
    midDistance_ = std::max(nearDistance_, midDistance);
    farDistance_ = std::max(midDistance_, farDistance);
}

OpenWorldCharacterLODResult OpenWorldCharacterLODPolicy::Evaluate(float distance, bool isInFrustum) const
{
    OpenWorldCharacterLODResult result;
    const float safeDistance = std::max(0.0f, distance);
    result.normalizedDistance = farDistance_ > 0.0f ? std::min(1.0f, safeDistance / farDistance_) : 1.0f;
    if (!isInFrustum || safeDistance > farDistance_)
    {
        result.level = OpenWorldCharacterLOD::Culled;
        result.castShadows = false;
    }
    else if (safeDistance > midDistance_)
    {
        result.level = OpenWorldCharacterLOD::Far;
        result.castShadows = false;
    }
    else if (safeDistance > nearDistance_)
    {
        result.level = OpenWorldCharacterLOD::Mid;
        result.castShadows = true;
    }
    else
    {
        result.level = OpenWorldCharacterLOD::Near;
        result.castShadows = true;
    }
    return result;
}

} // namespace Urho3D
