// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#include <catch2/catch_amalgamated.hpp>

#include <algorithm>

#include <Urho3D/RenderPipeline/ProductionRenderPipeline.h>

using namespace Urho3D;

namespace
{

bool Contains(const std::vector<std::string>& values, const std::string& value)
{
    return std::find(values.begin(), values.end(), value) != values.end();
}

} // namespace

TEST_CASE("Production render pipeline compiles deterministic feature schedule", "[render][production]")
{
    RenderGraph graph;
    ProductionRenderSettings settings;
    ProductionRenderPlan plan;
    std::string error;

    REQUIRE(ProductionRenderPipeline::Build(graph, settings, &plan, &error));
    REQUIRE(error.empty());
    REQUIRE(graph.IsCompiled());
    REQUIRE(plan.transientResourceCount > 0);
    REQUIRE(plan.transientAliasGroupCount > 0);
    REQUIRE_FALSE(plan.usesRayTracing);
    REQUIRE(Contains(plan.passNames, "VirtualizedGeometryClusterCull"));
    REQUIRE(Contains(plan.passNames, "GlobalIllumination"));
    REQUIRE(Contains(plan.passNames, "TemporalAntiAliasing"));
    REQUIRE(Contains(plan.passNames, "Present"));
    REQUIRE(graph.Execute(17));
}

TEST_CASE("Production render pipeline validates unsafe feature combinations", "[render][production][validation]")
{
    ProductionRenderSettings settings;
    std::string error;

    settings.width = 0;
    REQUIRE_FALSE(settings.Validate(&error));
    REQUIRE_FALSE(error.empty());

    settings = ProductionRenderSettings{};
    settings.enableRayTracing = true;
    settings.enableTemporalAntiAliasing = false;
    REQUIRE_FALSE(settings.Validate(&error));

    settings = ProductionRenderSettings{};
    settings.width = 32768;
    REQUIRE_FALSE(settings.Validate(&error));
}

TEST_CASE("Production render pipeline exposes optional ray tracing and post-process stages", "[render][production][raytracing]")
{
    RenderGraph graph;
    ProductionRenderSettings settings;
    settings.enableRayTracing = true;
    settings.enableDepthOfField = true;
    settings.enableMotionBlur = true;
    ProductionRenderPlan plan;
    std::string error;

    REQUIRE(ProductionRenderPipeline::Build(graph, settings, &plan, &error));
    REQUIRE(plan.usesRayTracing);
    REQUIRE(Contains(plan.passNames, "RayTracing"));
    REQUIRE(Contains(plan.passNames, "DepthOfField"));
    REQUIRE(Contains(plan.passNames, "MotionBlur"));

    settings.enableRayTracing = false;
    settings.enableGlobalIllumination = false;
    settings.enableVolumetrics = false;
    settings.enableScreenSpaceReflections = false;
    REQUIRE(ProductionRenderPipeline::Build(graph, settings, &plan, &error));
    REQUIRE_FALSE(plan.usesRayTracing);
    REQUIRE_FALSE(Contains(plan.passNames, "RayTracing"));
    REQUIRE_FALSE(Contains(plan.passNames, "GlobalIllumination"));
}
