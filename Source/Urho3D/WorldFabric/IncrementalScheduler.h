// Copyright (c) 2024-2026 robert-sarah and rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <EASTL/string.h>
#include <EASTL/unordered_map.h>
#include <EASTL/vector.h>

#include <Urho3D/Core/Attribute.h>

namespace Urho3D
{

enum class IncrementalTaskState
{
    Clean,
    Invalidated,
    Running,
    Succeeded,
    Failed
};

struct URHO3D_API IncrementalTask
{
    ea::string id;
    ea::string description;
    ea::vector<ea::string> dependencies;
    IncrementalTaskState state{IncrementalTaskState::Clean};
    unsigned revision{};
};

/// Deterministic invalidation scheduler for assets, shaders, scripts and packaging tasks.
class URHO3D_API IncrementalScheduler
{
public:
    bool RegisterTask(const IncrementalTask& task, ea::string* error = nullptr);
    bool AddDependency(const ea::string& taskId, const ea::string& dependencyId, ea::string* error = nullptr);
    bool Invalidate(const ea::string& taskId, ea::string* error = nullptr);

    bool MarkRunning(const ea::string& taskId, ea::string* error = nullptr);
    bool MarkSucceeded(const ea::string& taskId, ea::string* error = nullptr);
    bool MarkFailed(const ea::string& taskId, ea::string* error = nullptr);
    bool Reset(const ea::string& taskId, ea::string* error = nullptr);

    const IncrementalTask* GetTask(const ea::string& taskId) const;
    ea::vector<ea::string> GetReadyTasks() const;
    ea::vector<ea::string> GetInvalidatedTasks() const;
    const ea::unordered_map<ea::string, IncrementalTask>& GetTasks() const { return tasks_; }

    bool Validate(ea::string* error = nullptr) const;
    unsigned long long ComputeDigest(ea::string* error = nullptr) const;

private:
    bool HasPath(const ea::string& from, const ea::string& target, ea::unordered_map<ea::string, bool>& visited) const;
    bool SetState(const ea::string& taskId, IncrementalTaskState state, ea::string* error);
    void InvalidateDependents(const ea::string& taskId);

    ea::unordered_map<ea::string, IncrementalTask> tasks_;
};

}
