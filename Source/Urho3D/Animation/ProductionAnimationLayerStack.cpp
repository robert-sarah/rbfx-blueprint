// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#include "ProductionAnimationLayerStack.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace Urho3D
{

bool ProductionAnimationLayerStack::SetLayer(const ProductionAnimationLayer& layer)
{
    if (layer.name.empty() || layer.clip.empty() || !std::isfinite(layer.weight) || layer.weight < 0.0f)
        return false;

    ProductionAnimationLayer normalized = layer;
    normalized.weight = std::min(layer.weight, 1.0f);
    layers_[normalized.name] = std::move(normalized);
    return true;
}

bool ProductionAnimationLayerStack::RemoveLayer(const std::string& name)
{
    return layers_.erase(name) != 0;
}

void ProductionAnimationLayerStack::Clear()
{
    layers_.clear();
}

std::vector<ProductionAnimationContribution> ProductionAnimationLayerStack::Evaluate() const
{
    float baseWeightSum = 0.0f;
    for (const auto& entry : layers_)
    {
        if (entry.second.enabled && !entry.second.additive)
            baseWeightSum += entry.second.weight;
    }

    std::vector<ProductionAnimationContribution> result;
    result.reserve(layers_.size());
    for (const auto& entry : layers_)
    {
        const ProductionAnimationLayer& layer = entry.second;
        if (!layer.enabled || layer.additive || layer.weight <= 0.0f)
            continue;

        const float evaluatedWeight = baseWeightSum > 0.0f ? layer.weight / baseWeightSum : 0.0f;
        result.push_back({layer.name, layer.clip, evaluatedWeight, false});
    }
    for (const auto& entry : layers_)
    {
        const ProductionAnimationLayer& layer = entry.second;
        if (!layer.enabled || !layer.additive || layer.weight <= 0.0f)
            continue;
        result.push_back({layer.name, layer.clip, layer.weight, true});
    }
    return result;
}

const ProductionAnimationLayer* ProductionAnimationLayerStack::GetLayer(const std::string& name) const
{
    const auto it = layers_.find(name);
    return it != layers_.end() ? &it->second : nullptr;
}

} // namespace Urho3D

