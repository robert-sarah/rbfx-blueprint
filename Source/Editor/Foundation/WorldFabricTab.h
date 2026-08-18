#pragma once

#include "../Project/ResourceEditorTab.h"

#include <Urho3D/WorldFabric/WorldFabricGraphResource.h>

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
};

} // namespace Urho3D
