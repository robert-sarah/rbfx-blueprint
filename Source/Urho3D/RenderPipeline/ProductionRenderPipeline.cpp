// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#include "ProductionRenderPipeline.h"

#include <algorithm>
#include <sstream>

namespace Urho3D
{

namespace
{

bool Fail(std::string* error, const char* message)
{
    if (error)
        *error = message;
    return false;
}

RenderGraphResourceHandle CreateTexture(RenderGraph& graph, const char* name, unsigned width, unsigned height,
    bool transient = true)
{
    RenderGraphResourceDesc desc;
    desc.name = name;
    desc.kind = RenderGraphResourceKind::Texture;
    desc.width = width;
    desc.height = height;
    desc.depth = 1;
    desc.format = 1;
    desc.bindFlags = 1;
    desc.transient = transient;
    return graph.CreateResource(desc);
}

RenderGraphPassHandle AddPass(RenderGraph& graph, const char* name,
    std::initializer_list<RenderGraphResourceUse> resources)
{
    RenderGraphPassDesc pass;
    pass.name = name;
    for (const RenderGraphResourceUse& resource : resources)
        pass.resources.push_back(resource);
    pass.execute = [](const RenderGraphPassContext&) {};
    return graph.AddPass(pass);
}

RenderGraphResourceUse Read(RenderGraphResourceHandle resource)
{
    return RenderGraphResourceUse{resource, false};
}

RenderGraphResourceUse Write(RenderGraphResourceHandle resource)
{
    return RenderGraphResourceUse{resource, true};
}

RenderGraphResourceHandle AddColorStage(RenderGraph& graph, const char* name,
    RenderGraphResourceHandle input, RenderGraphResourceHandle depth, unsigned width, unsigned height)
{
    const RenderGraphResourceHandle output = CreateTexture(graph, name, width, height);
    AddPass(graph, name, {Read(input), Read(depth), Write(output)});
    return output;
}

} // namespace

bool ProductionRenderSettings::Validate(std::string* error) const
{
    if (width == 0 || height == 0)
        return Fail(error, "ProductionRenderSettings requires non-zero dimensions");
    if (width > 16384 || height > 16384)
        return Fail(error, "ProductionRenderSettings dimensions exceed the safe 16K planning limit");
    if (enableRayTracing && (!enableTextureStreaming || !enableTemporalAntiAliasing))
        return Fail(error, "Ray tracing plans require texture streaming and temporal accumulation");
    if (enableScreenSpaceReflections && !enableTemporalAntiAliasing)
        return Fail(error, "Screen-space reflections require temporal anti-aliasing in the production plan");
    return true;
}

bool ProductionRenderPipeline::Build(RenderGraph& graph, const ProductionRenderSettings& settings,
    ProductionRenderPlan* plan, std::string* error)
{
    if (plan)
        *plan = ProductionRenderPlan{};
    if (!settings.Validate(error))
        return false;

    graph.Reset();

    const RenderGraphResourceHandle depth = CreateTexture(graph, "SceneDepth", settings.width, settings.height, false);
    const RenderGraphResourceHandle geometry = CreateTexture(graph, "GeometryVisibility", settings.width, settings.height);
    const RenderGraphResourceHandle shadow = settings.enableShadows
        ? CreateTexture(graph, "ShadowAtlas", 4096, 4096)
        : InvalidRenderGraphResource;
    const RenderGraphResourceHandle sceneColor = CreateTexture(graph, "SceneColor", settings.width, settings.height);

    if (settings.enableVirtualizedGeometry)
    {
        AddPass(graph, "VirtualizedGeometryClusterCull", {Write(geometry)});
    }
    else
    {
        AddPass(graph, "StaticGeometryVisibility", {Write(geometry)});
    }
    AddPass(graph, "DepthPrepass", {Read(geometry), Write(depth)});

    if (settings.enableShadows)
        AddPass(graph, "ShadowMaps", {Read(geometry), Write(shadow)});

    ea::vector<RenderGraphResourceUse> opaqueResources;
    opaqueResources.push_back(Read(depth));
    if (settings.enableShadows)
        opaqueResources.push_back(Read(shadow));
    opaqueResources.push_back(Write(sceneColor));
    RenderGraphPassDesc opaque;
    opaque.name = "OpaqueLighting";
    opaque.resources = opaqueResources;
    opaque.execute = [](const RenderGraphPassContext&) {};
    graph.AddPass(opaque);

    if (settings.enableTerrain)
        AddPass(graph, "Terrain", {Read(depth), Write(sceneColor)});
    if (settings.enableVegetation)
        AddPass(graph, "Vegetation", {Read(depth), Write(sceneColor)});
    if (settings.enableWater)
        AddPass(graph, "Water", {Read(depth), Write(sceneColor)});

    RenderGraphResourceHandle color = sceneColor;
    if (settings.enableGlobalIllumination)
        color = AddColorStage(graph, "GlobalIllumination", color, depth, settings.width, settings.height);
    if (settings.enableAtmosphere)
        color = AddColorStage(graph, "Atmosphere", color, depth, settings.width, settings.height);
    if (settings.enableVolumetrics)
        color = AddColorStage(graph, "Volumetrics", color, depth, settings.width, settings.height);
    if (settings.enableRayTracing)
        color = AddColorStage(graph, "RayTracing", color, depth, settings.width, settings.height);
    if (settings.enableScreenSpaceReflections)
        color = AddColorStage(graph, "ScreenSpaceReflections", color, depth, settings.width, settings.height);
    if (settings.enableTemporalAntiAliasing)
        color = AddColorStage(graph, "TemporalAntiAliasing", color, depth, settings.width, settings.height);
    if (settings.enableDepthOfField)
        color = AddColorStage(graph, "DepthOfField", color, depth, settings.width, settings.height);
    if (settings.enableMotionBlur)
        color = AddColorStage(graph, "MotionBlur", color, depth, settings.width, settings.height);

    if (settings.enableTextureStreaming)
        AddPass(graph, "TextureResidency", {Read(color)});
    const RenderGraphResourceHandle toneMapped = CreateTexture(graph, "ToneMappedColor", settings.width, settings.height);
    AddPass(graph, "ToneMapping", {Read(color), Write(toneMapped)});
    AddPass(graph, "Present", {Read(toneMapped)});

    if (!graph.Compile())
        return Fail(error, graph.GetLastError().c_str());

    if (plan)
    {
        plan->usesRayTracing = settings.enableRayTracing;
        plan->transientResourceCount = 0;
        plan->transientAliasGroupCount = graph.GetTransientAliasGroupCount();
        for (const RenderGraphCompiledResource& resource : graph.GetCompiledResources())
        {
            if (resource.desc.transient)
                ++plan->transientResourceCount;
        }
        for (RenderGraphPassHandle handle : graph.GetExecutionOrder())
        {
            // Pass handles are declaration-stable and names are available through the
            // graph only through the resource-independent plan; keep this list explicit.
            (void)handle;
        }
        plan->passNames = {
            "DepthPrepass",
            settings.enableVirtualizedGeometry ? "VirtualizedGeometryClusterCull" : "StaticGeometryVisibility",
            settings.enableShadows ? "ShadowMaps" : "",
            "OpaqueLighting",
            settings.enableTerrain ? "Terrain" : "",
            settings.enableVegetation ? "Vegetation" : "",
            settings.enableWater ? "Water" : "",
            settings.enableGlobalIllumination ? "GlobalIllumination" : "",
            settings.enableAtmosphere ? "Atmosphere" : "",
            settings.enableVolumetrics ? "Volumetrics" : "",
            settings.enableRayTracing ? "RayTracing" : "",
            settings.enableScreenSpaceReflections ? "ScreenSpaceReflections" : "",
            settings.enableTemporalAntiAliasing ? "TemporalAntiAliasing" : "",
            settings.enableDepthOfField ? "DepthOfField" : "",
            settings.enableMotionBlur ? "MotionBlur" : "",
            settings.enableTextureStreaming ? "TextureResidency" : "",
            "ToneMapping",
            "Present",
        };
        plan->passNames.erase(std::remove(plan->passNames.begin(), plan->passNames.end(), ""), plan->passNames.end());
    }
    return true;
}

} // namespace Urho3D
