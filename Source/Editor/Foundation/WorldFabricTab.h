#pragma once

#include "../Project/ResourceEditorTab.h"

#include <Urho3D/WorldFabric/CausalWorldFabricDebugger.h>
#include <Urho3D/WorldFabric/DeterministicSimulation.h>
#include <Urho3D/WorldFabric/SemanticBuildCapsule.h>
#include <Urho3D/WorldFabric/UniversalDeterministicTimeMachine.h>
#include <Urho3D/WorldFabric/WorldFabricCollaboration.h>
#include <Urho3D/WorldFabric/WorldFabricGraphResource.h>
#include <Urho3D/WorldFabric/WorldFabricProfiler.h>

namespace Urho3D
{

class Project;

void Foundation_WorldFabricTab(Context* context, Project* project);

class WorldFabricTab final : public ResourceEditorTab
{
    URHO3D_OBJECT(WorldFabricTab, ResourceEditorTab);

public:
    explicit WorldFabricTab(Context* context);

    bool CanOpenResource(const ResourceFileDescriptor& desc) override;
    bool SupportMultipleResources() override { return false; }
    ea::string GetResourceTitle() override { return "World Fabric Dependency Explorer"; }
    bool IsUndoSupported() override { return true; }

    WorldFabricGraphResource& GetWorldFabric();
    const WorldFabricGraphResource& GetWorldFabric() const;
    JSONValue CaptureGraph() const;
    void ApplyGraphSnapshot(const JSONValue& snapshot);

protected:
    void RenderToolbar() override;
    void RenderContent() override;
    void RenderContextMenuItems() override;
    void OnResourceLoaded(const ea::string& resourceName) override;
    void OnResourceUnloaded(const ea::string& resourceName) override;
    void OnActiveResourceChanged(const ea::string& previousResourceName, const ea::string& newResourceName) override;
    void OnResourceSaved(const ea::string& resourceName) override;
    void OnResourceShallowSaved(const ea::string& resourceName) override;

private:
    void CommitGraphEdit(const JSONValue& before, const ea::string& status);
    void ResetTemplate();
    void SeedReflection();
    void ValidateGraph();
    void AddNode();
    void RemoveSelectedNode();
    void AddDependency();
    void RemoveSelectedDependency();
    void RenderNodes(WorldFabricGraphResource& resource);
    void RenderNodeInspector(WorldFabricGraphResource& resource);
    void RenderDependencies(WorldFabricGraphResource& resource);
    void RenderBuildOrder(const WorldFabricGraphResource& resource);
    void RenderImpactAnalysis(const WorldFabricGraphResource& resource);
    void RenderSemanticQuery(const WorldFabricGraphResource& resource);
    void RenderProfiler(const WorldFabricGraphResource& resource);
    void RenderCollaboration(const WorldFabricGraphResource& resource);
    void RenderDeterministicReproduction(const WorldFabricGraphResource& resource);
    void RenderCausalDebugger(const WorldFabricGraphResource& resource);
    void RenderTimeMachine(const WorldFabricGraphResource& resource);
    void RenderBuildCapsule(const WorldFabricGraphResource& resource);

    SharedPtr<WorldFabricGraphResource> resource_;
    WorldFabricGraphResource preview_;
    ea::string selectedNodeKey_;
    ea::string selectedDependencyLabel_;
    ea::string newNodeKey_;
    ea::string newNodeType_;
    ea::string dependencyNodeKey_;
    ea::string dependencyTargetKey_;
    ea::string dependencyLabel_;
    unsigned newNodeKind_{static_cast<unsigned>(WorldFabricNodeKind::Custom)};
    unsigned dependencyKind_{static_cast<unsigned>(WorldFabricDependencyKind::Requires)};
    ea::string status_;
    ea::string validationError_;
    ea::string semanticQuery_;
    ea::string collaborationClientId_{"editor"};
    ea::string newCollaborationClientId_;
    ea::string deterministicStatus_;
    float deterministicFixedDelta_{1.0f / 60.0f};
    unsigned deterministicCapacity_{128};
    unsigned deterministicTargetFrame_{};
    WorldFabricProfiler profiler_;
    WorldFabricCollaboration collaboration_;
    DeterministicSimulation deterministicSimulation_;
    CausalWorldFabricDebugger causalDebugger_;
    UniversalDeterministicTimeMachine deterministicTimeMachine_;
    SemanticBuildCapsule semanticBuildCapsule_;
    ea::string causalStatus_;
    ea::string capsuleStatus_;
    unsigned long long capsuleDigest_{};
    int timeMachineStepDelta_{1};
};

} // namespace Urho3D
