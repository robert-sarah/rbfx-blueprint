// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include "../Core/StringUtils.h"

#include <map>
#include <string>
#include <vector>

namespace Urho3D
{

struct URHO3D_API ProductionAnimationLayer
{
    std::string name;
    std::string clip;
    float weight{1.0f};
    bool additive{};
    bool enabled{true};
};

struct URHO3D_API ProductionAnimationContribution
{
    std::string layer;
    std::string clip;
    float weight{};
    bool additive{};
};

/// Stable animation layer evaluator used before the renderer applies poses.
/// Base layers are normalized together; additive layers preserve their authored
/// weight and are emitted after base layers in lexical layer order.
class URHO3D_API ProductionAnimationLayerStack
{
public:
    bool SetLayer(const ProductionAnimationLayer& layer);
    bool RemoveLayer(const std::string& name);
    void Clear();

    std::vector<ProductionAnimationContribution> Evaluate() const;
    const ProductionAnimationLayer* GetLayer(const std::string& name) const;
    unsigned GetLayerCount() const { return static_cast<unsigned>(layers_.size()); }

private:
    std::map<std::string, ProductionAnimationLayer> layers_;
};

} // namespace Urho3D

