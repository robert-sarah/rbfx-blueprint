// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#include "ProductionBehaviorTree.h"

#include <algorithm>

namespace Urho3D
{

namespace
{
void SetError(std::string* error, const char* message)
{
    if (error)
        *error = message;
}
}

bool ProductionBehaviorTree::AddNode(const ProductionBehaviorNode& node, std::string* error)
{
    if (node.id.empty())
    {
        SetError(error, "behavior node id must not be empty");
        return false;
    }
    if (nodes_.find(node.id) != nodes_.end())
    {
        SetError(error, "behavior node id already exists");
        return false;
    }
    if (node.type == ProductionBehaviorNodeType::Condition && node.key.empty())
    {
        SetError(error, "condition node key must not be empty");
        return false;
    }
    if (node.parentId.empty())
    {
        if (!rootId_.empty())
        {
            SetError(error, "behavior tree can have only one root");
            return false;
        }
        if (node.type == ProductionBehaviorNodeType::Condition || node.type == ProductionBehaviorNodeType::Action)
        {
            SetError(error, "behavior tree root must be a sequence or selector");
            return false;
        }
    }
    else
    {
        const auto parentIt = nodes_.find(node.parentId);
        if (parentIt == nodes_.end())
        {
            SetError(error, "behavior node parent does not exist");
            return false;
        }
        if (parentIt->second.definition.type == ProductionBehaviorNodeType::Condition
            || parentIt->second.definition.type == ProductionBehaviorNodeType::Action)
        {
            SetError(error, "condition and action nodes cannot have children");
            return false;
        }
    }

    NodeState state;
    state.definition = node;
    nodes_.emplace(node.id, std::move(state));
    if (node.parentId.empty())
        rootId_ = node.id;
    else
        nodes_.at(node.parentId).children.push_back(node.id);
    return true;
}

bool ProductionBehaviorTree::RemoveNode(const std::string& id)
{
    const auto rootIt = nodes_.find(id);
    if (rootIt == nodes_.end())
        return false;

    std::vector<std::string> pending{id};
    std::vector<std::string> removed;
    while (!pending.empty())
    {
        const std::string current = pending.back();
        pending.pop_back();
        const auto it = nodes_.find(current);
        if (it == nodes_.end())
            continue;
        removed.push_back(current);
        pending.insert(pending.end(), it->second.children.begin(), it->second.children.end());
    }

    const std::string parentId = rootIt->second.definition.parentId;
    if (!parentId.empty())
    {
        auto parentIt = nodes_.find(parentId);
        if (parentIt != nodes_.end())
        {
            auto& children = parentIt->second.children;
            children.erase(std::remove(children.begin(), children.end(), id), children.end());
        }
    }
    for (const std::string& removedId : removed)
    {
        nodes_.erase(removedId);
        actions_.erase(removedId);
    }
    if (rootId_ == id)
        rootId_.clear();
    return true;
}

void ProductionBehaviorTree::Clear()
{
    nodes_.clear();
    conditions_.clear();
    actions_.clear();
    rootId_.clear();
}

bool ProductionBehaviorTree::SetCondition(const std::string& key, bool value)
{
    if (key.empty())
        return false;
    conditions_[key] = value;
    return true;
}

bool ProductionBehaviorTree::SetActionStatus(const std::string& id, ProductionBehaviorStatus status)
{
    const auto it = nodes_.find(id);
    if (it == nodes_.end() || it->second.definition.type != ProductionBehaviorNodeType::Action)
        return false;
    actions_[id] = status;
    return true;
}

ProductionBehaviorStatus ProductionBehaviorTree::Tick() const
{
    return rootId_.empty() ? ProductionBehaviorStatus::Failure : Evaluate(rootId_);
}

const ProductionBehaviorNode* ProductionBehaviorTree::GetNode(const std::string& id) const
{
    const auto it = nodes_.find(id);
    return it != nodes_.end() ? &it->second.definition : nullptr;
}

ProductionBehaviorStatus ProductionBehaviorTree::Evaluate(const std::string& id) const
{
    const auto it = nodes_.find(id);
    if (it == nodes_.end())
        return ProductionBehaviorStatus::Failure;

    const ProductionBehaviorNode& node = it->second.definition;
    if (node.type == ProductionBehaviorNodeType::Condition)
    {
        const auto conditionIt = conditions_.find(node.key);
        return conditionIt != conditions_.end() && conditionIt->second
            ? ProductionBehaviorStatus::Success
            : ProductionBehaviorStatus::Failure;
    }
    if (node.type == ProductionBehaviorNodeType::Action)
    {
        const auto actionIt = actions_.find(node.id);
        return actionIt != actions_.end() ? actionIt->second : ProductionBehaviorStatus::Failure;
    }

    std::vector<std::string> children = it->second.children;
    std::sort(children.begin(), children.end());
    if (node.type == ProductionBehaviorNodeType::Sequence)
    {
        for (const std::string& child : children)
        {
            const ProductionBehaviorStatus status = Evaluate(child);
            if (status != ProductionBehaviorStatus::Success)
                return status;
        }
        return ProductionBehaviorStatus::Success;
    }

    for (const std::string& child : children)
    {
        const ProductionBehaviorStatus status = Evaluate(child);
        if (status == ProductionBehaviorStatus::Success)
            return status;
        if (status == ProductionBehaviorStatus::Running)
            return status;
    }
    return ProductionBehaviorStatus::Failure;
}

} // namespace Urho3D
