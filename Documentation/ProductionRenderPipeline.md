# Production Render Pipeline

`ProductionRenderPipeline` is the deterministic, backend-neutral scheduling layer for the engine's production render features. It compiles a concrete [`RenderGraph`](../Source/Urho3D/RenderPipeline/RenderGraph.h) rather than pretending that a feature is complete merely because a setting exists.

## What is implemented

The planner creates inspectable resources and ordered passes for depth, shadows, visibility, virtualized-geometry cluster culling, terrain, vegetation, water, baked/runtime global-illumination integration, atmosphere, volumetrics, optional ray tracing, screen-space reflections, temporal anti-aliasing, depth of field, motion blur, texture-residency tracking, tone mapping and presentation.

| Capability | RenderGraph contract | Runtime responsibility |
|---|---|---|
| Virtualized geometry | `VirtualizedGeometryClusterCull` pass and `GeometryVisibility` resource | The renderer backend supplies cluster buffers, culling shaders and draw submission. |
| Texture streaming / residency | `TextureResidency` pass and explicit resource lifetime | `TextureStreamingManager` produces deterministic mip residency decisions under a strict byte budget; the graphics backend supplies mip upload, residency eviction and synchronization. |
| Terrain, vegetation and water | Dedicated ordered passes that consume depth and write scene color | Feature renderers supply terrain patches, vegetation instances and water materials. |
| Global illumination | `GlobalIllumination` stage after opaque lighting | Existing `GlobalIllumination` components supply baked probes; a runtime GI backend can consume the same stage. |
| Atmosphere and volumetrics | Separate color stages with depth input | The atmosphere/fog implementation supplies LUTs, froxel resources or compute dispatches. |
| Ray tracing | Optional `RayTracing` stage guarded by validation | A platform backend must provide acceleration structures and ray-tracing shaders. The planner does not claim software ray tracing. |
| TAA/SSR/DOF/motion blur | Ordered post-process stages with explicit transient resources | The post-process backend supplies history, reprojection and shader implementations. |
| LOD/HLOD/impostors | Visibility stage boundary and resource scheduling | `LODGroup`, `WorldPartition` and future HLOD/impostor producers feed the visibility pass. |

This separation is intentional. The planner provides real dependency ordering, resource hazards, transient aliasing and a stable inspection surface. It does not hide missing hardware-specific shader work behind a boolean flag.

## Building a plan

```cpp
#include <Urho3D/RenderPipeline/ProductionRenderPipeline.h>

RenderGraph graph;
ProductionRenderSettings settings;
settings.enableRayTracing = false;
settings.enableDepthOfField = true;

ProductionRenderPlan plan;
std::string error;
if (!ProductionRenderPipeline::Build(graph, settings, &plan, &error))
    LogError(error.c_str());
else
    graph.Execute(frameIndex);
```

`ProductionRenderSettings::Validate` rejects zero-sized targets, dimensions above the safe 16K planning limit, ray-tracing plans without temporal accumulation or texture streaming, and screen-space reflections without temporal anti-aliasing. These checks make invalid combinations fail before a GPU backend is touched.

## Determinism and inspection

Passes are declared in a stable order. `RenderGraph::Compile` resolves read/write hazards, rejects cycles, computes an execution order and calculates transient alias groups. The returned `ProductionRenderPlan` records the feature passes and whether the plan uses ray tracing, which makes it suitable for editor diagnostics, production readiness gates and CI tests.

The actual GPU implementation remains backend-specific. A production platform adapter must connect each pass to its shaders, barriers, history textures, streaming queues and profiling labels. `TextureStreamingManager` is intentionally transport- and backend-neutral: it validates registrations, sorts requests by priority and resource name, and emits a reproducible target mip plus load/evict decision for each frame. The engine therefore exposes an honest maturity boundary: **scheduling and validation are implemented; hardware-specific render backends must still be certified per platform**.

## Validation

The `TestProductionRenderPipeline` suite verifies that the default schedule compiles and executes, unsafe combinations are rejected, transient resources are planned, and optional ray-tracing/post-process stages appear or disappear deterministically.
