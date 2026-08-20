// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include "RenderGraph.h"

#include <string>
#include <vector>

namespace Urho3D
{

enum class ProductionRenderFeature
{
    DepthPrepass,
    ShadowMaps,
    TextureResidency,
    VirtualizedGeometry,
    Terrain,
    Vegetation,
    Water,
    GlobalIllumination,
    Atmosphere,
    Volumetrics,
    RayTracing,
    TemporalAntiAliasing,
    ScreenSpaceReflections,
    DepthOfField,
    MotionBlur,
    ToneMapping,
    Present,
};

/// Backend-neutral settings for the production render plan.
struct URHO3D_API ProductionRenderSettings
{
    unsigned width{1280};
    unsigned height{720};
    bool enableShadows{true};
    bool enableTextureStreaming{true};
    bool enableVirtualizedGeometry{true};
    bool enableTerrain{true};
    bool enableVegetation{true};
    bool enableWater{true};
    bool enableGlobalIllumination{true};
    bool enableAtmosphere{true};
    bool enableVolumetrics{true};
    bool enableRayTracing{};
    bool enableTemporalAntiAliasing{true};
    bool enableScreenSpaceReflections{true};
    bool enableDepthOfField{};
    bool enableMotionBlur{};

    bool Validate(std::string* error = nullptr) const;
};

/// Result of compiling a production feature plan into RenderGraph.
struct URHO3D_API ProductionRenderPlan
{
    std::vector<std::string> passNames;
    unsigned transientResourceCount{};
    unsigned transientAliasGroupCount{};
    bool usesRayTracing{};
};

/// Builds a deterministic, inspectable render schedule on top of RenderGraph.
/// Backend code owns actual GPU allocations and shader dispatches; this class owns
/// feature ordering, resource hazards and transient lifetime planning.
class URHO3D_API ProductionRenderPipeline
{
public:
    static bool Build(RenderGraph& graph, const ProductionRenderSettings& settings,
        ProductionRenderPlan* plan = nullptr, std::string* error = nullptr);
};

} // namespace Urho3D
