// Copyright (c) 2024-2026 robert-sarah and rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#include "IncrementalScheduler.h"

#include <Urho3D/Core/StringUtils.h>

#include <algorithm>

namespace Urho3D
{
namespace
{

const char* StateName(IncrementalTaskState state)
{
    switch (state)
    {
    case IncrementalTaskState::Clean: return "clean";
    case IncrementalTaskState::Invalidated: return "invalidated";
    case IncrementalTaskState::Running: return "running";
    case IncrementalTaskState::Succeeded: return "succeeded";
    case IncrementalTaskState::Failed: return "failed";
    }
    return "unknown";
}

void SetError(ea::string* error, const ea::string& message)
{
    if (error)
        *error = message;
}

unsigned long long HashText(const ea::string& text)
{
    unsigned long long digest = 1469598103934665603ull;
    for (const unsigned char character : text)
    {
        digest ^= character;
        digest *= 1099511628211ull;
    }
    return digest;
}

}

bool IncrementalScheduler::RegisterTask(const IncrementalTask& task, ea::string* error)
{
    if (task.id.empty())
    {
        SetError(error, "Incremental task id must not be empty");
        return false;
    }
    if (tasks_.contains(task.id))
    {
        SetError(error, "Incremental task is already registered: " + task.id);
        return false;
    }
    IncrementalTask copy = task;
    copy.state = IncrementalTaskState::Clean;
    tasks_.emplace(copy.id, ea::move(copy));
    return Validate(error);
}

bool IncrementalScheduler::AddDependency(const ea::string& taskId, const ea::string& dependencyId, ea::string* error)
{
    auto taskIt = tasks_.find(taskId);
    if (taskIt == tasks_.end())
    {
        SetError(error, "Unknown incremental task: " + taskId);
        return false;
    }
    if (tasks_.find(dependencyId) == tasks_.end())
    {
        SetError(error, "Unknown incremental dependency: " + dependencyId);
        return false;
    }
    if (taskId == dependencyId)
    {
        SetError(error, "Incremental task cannot depend on itself: " + taskId);
        return false;
    }
    if (std::find(taskIt->second.dependencies.begin(), taskIt->second.dependencies.end(), dependencyId)
        != taskIt->second.dependencies.end())
    {
        return true;
    }

    taskIt->second.dependencies.push_back(dependencyId);
    if (!Validate(error))
    {
        taskIt->second.dependencies.pop_back();
        return false;
    }
    return true;
}

bool IncrementalScheduler::Invalidate(const ea::string& taskId, ea::string* error)
{
    auto taskIt = tasks_.find(taskId);
    if (taskIt == tasks_.end())
    {
        SetError(error, "Unknown incremental task: " + taskId);
        return false;
    }
    taskIt->second.state = IncrementalTaskState::Invalidated;
    ++taskIt->second.revision;
    InvalidateDependents(taskId);
    return true;
}

bool IncrementalScheduler::MarkRunning(const ea::string& taskId, ea::string* error)
{
    return SetState(taskId, IncrementalTaskState::Running, error);
}

bool IncrementalScheduler::MarkSucceeded(const ea::string& taskId, ea::string* error)
{
    return SetState(taskId, IncrementalTaskState::Succeeded, error);
}

bool IncrementalScheduler::MarkFailed(const ea::string& taskId, ea::string* error)
{
    return SetState(taskId, IncrementalTaskState::Failed, error);
}

bool IncrementalScheduler::Reset(const ea::string& taskId, ea::string* error)
{
    return SetState(taskId, IncrementalTaskState::Clean, error);
}

const IncrementalTask* IncrementalScheduler::GetTask(const ea::string& taskId) const
{
    const auto it = tasks_.find(taskId);
    return it != tasks_.end() ? &it->second : nullptr;
}

ea::vector<ea::string> IncrementalScheduler::GetReadyTasks() const
{
    ea::vector<ea::string> result;
    for (const auto& pair : tasks_)
    {
        const IncrementalTask& task = pair.second;
        if (task.state != IncrementalTaskState::Invalidated)
            continue;

        bool ready = true;
        for (const ea::string& dependencyId : task.dependencies)
        {
            const auto dependencyIt = tasks_.find(dependencyId);
            if (dependencyIt == tasks_.end()
                || (dependencyIt->second.state != IncrementalTaskState::Clean
                    && dependencyIt->second.state != IncrementalTaskState::Succeeded))
            {
                ready = false;
                break;
            }
        }
        if (ready)
            result.push_back(task.id);
    }
    std::sort(result.begin(), result.end());
    return result;
}

ea::vector<ea::string> IncrementalScheduler::GetInvalidatedTasks() const
{
    ea::vector<ea::string> result;
    for (const auto& pair : tasks_)
    {
        if (pair.second.state == IncrementalTaskState::Invalidated)
            result.push_back(pair.first);
    }
    std::sort(result.begin(), result.end());
    return result;
}

bool IncrementalScheduler::Validate(ea::string* error) const
{
    for (const auto& pair : tasks_)
    {
        const IncrementalTask& task = pair.second;
        if (task.id.empty())
        {
            SetError(error, "Incremental task id must not be empty");
            return false;
        }
        ea::unordered_map<ea::string, bool> visited;
        if (HasPath(task.id, task.id, visited))
        {
            SetError(error, "Incremental task dependency cycle detected at: " + task.id);
            return false;
        }
        for (const ea::string& dependencyId : task.dependencies)
        {
            if (tasks_.find(dependencyId) == tasks_.end())
            {
                SetError(error, "Incremental task has unknown dependency: " + dependencyId);
                return false;
            }
        }
    }
    return true;
}

unsigned long long IncrementalScheduler::ComputeDigest(ea::string* error) const
{
    if (!Validate(error))
        return 0;

    ea::vector<ea::string> ids;
    ids.reserve(tasks_.size());
    for (const auto& pair : tasks_)
        ids.push_back(pair.first);
    std::sort(ids.begin(), ids.end());

    ea::string serialized = "incremental-scheduler-v1\\n";
    for (const ea::string& id : ids)
    {
        const IncrementalTask& task = tasks_.at(id);
        serialized += task.id + "|" + task.description + "|" + StateName(task.state)
            + "|" + ToString("%u", task.revision) + "|";
        ea::vector<ea::string> dependencies = task.dependencies;
        std::sort(dependencies.begin(), dependencies.end());
        for (const ea::string& dependency : dependencies)
            serialized += dependency + ",";
        serialized += "\\n";
    }
    return HashText(serialized);
}

bool IncrementalScheduler::HasPath(const ea::string& from, const ea::string& target,
    ea::unordered_map<ea::string, bool>& visited) const
{
    const auto taskIt = tasks_.find(from);
    if (taskIt == tasks_.end())
        return false;

    for (const ea::string& dependencyId : taskIt->second.dependencies)
    {
        if (dependencyId == target)
            return true;
        if (visited[dependencyId])
            continue;
        visited[dependencyId] = true;
        if (HasPath(dependencyId, target, visited))
            return true;
    }
    return false;
}

bool IncrementalScheduler::SetState(const ea::string& taskId, IncrementalTaskState state, ea::string* error)
{
    auto taskIt = tasks_.find(taskId);
    if (taskIt == tasks_.end())
    {
        SetError(error, "Unknown incremental task: " + taskId);
        return false;
    }
    taskIt->second.state = state;
    ++taskIt->second.revision;
    return true;
}

void IncrementalScheduler::InvalidateDependents(const ea::string& taskId)
{
    for (auto& pair : tasks_)
    {
        IncrementalTask& task = pair.second;
        if (std::find(task.dependencies.begin(), task.dependencies.end(), taskId) == task.dependencies.end())
            continue;
        if (task.state != IncrementalTaskState::Invalidated)
        {
            task.state = IncrementalTaskState::Invalidated;
            ++task.revision;
        }
        InvalidateDependents(task.id);
    }
}

}
