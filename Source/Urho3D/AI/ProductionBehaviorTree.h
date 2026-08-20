// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include "../Core/StringUtils.h"

#include <map>
#include <string>
#include <vector>

namespace Urho3D
{

enum class ProductionBehaviorNodeType
{
    Sequence,
    Selector,
    Condition,
    Action
};

enum class ProductionBehaviorStatus
{
    Failure,
    Success,
    Running
};

struct URHO3D_API ProductionBehaviorNode
{
    std::string id;
    std::string parentId;
    ProductionBehaviorNodeType type{ProductionBehaviorNodeType::Sequence};
    std::string key;
};

/// Small deterministic behavior tree runtime suitable for gameplay and tests.
/// Conditions read a stable blackboard and actions expose their current result;
/// a game-specific adapter can replace those results from real gameplay code.
class URHO3D_API ProductionBehaviorTree
{
public:
    bool AddNode(const ProductionBehaviorNode& node, std::string* error = nullptr);
    bool RemoveNode(const std::string& id);
    void Clear();

    bool SetCondition(const std::string& key, bool value);
    bool SetActionStatus(const std::string& id, ProductionBehaviorStatus status);
    ProductionBehaviorStatus Tick() const;

    const ProductionBehaviorNode* GetNode(const std::string& id) const;
    const std::string& GetRootId() const { return rootId_; }
    unsigned GetNodeCount() const { return static_cast<unsigned>(nodes_.size()); }

private:
    struct NodeState
    {
        ProductionBehaviorNode definition;
        std::vector<std::string> children;
    };

    ProductionBehaviorStatus Evaluate(const std::string& id) const;

    std::map<std::string, NodeState> nodes_;
    std::map<std::string, bool> conditions_;
    std::map<std::string, ProductionBehaviorStatus> actions_;
    std::string rootId_;
};

} // namespace Urho3D
