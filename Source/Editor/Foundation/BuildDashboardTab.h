// SPDX-License-Identifier: MIT
#pragma once

#include "../Project/ResourceEditorTab.h"

#include <Urho3D/WorldFabric/BuildDashboardResource.h>

namespace Urho3D
{

void Foundation_BuildDashboardTab(Context* context, Project* project);

/// Editor for deterministic asset/shader/script/VFX/package build graphs.
class BuildDashboardTab : public ResourceEditorTab
{
    URHO3D_OBJECT(BuildDashboardTab, ResourceEditorTab);

public:
    explicit BuildDashboardTab(Context* context);

    void RenderContent() override;
    void RenderToolbar() override;
    void RenderContextMenuItems() override;
    bool CanOpenResource(const ResourceFileDescriptor& desc) override;
    bool SupportMultipleResources() override { return false; }
    ea::string GetResourceTitle() override { return "Build Dashboard"; }
    bool IsUndoSupported() override { return true; }

    void ApplyDashboardSnapshot(const JSONValue& snapshot);

protected:
    void OnResourceLoaded(const ea::string& resourceName) override;
    void OnResourceUnloaded(const ea::string& resourceName) override;
    void OnActiveResourceChanged(const ea::string& oldResourceName, const ea::string& newResourceName) override;
    void OnResourceSaved(const ea::string& resourceName) override;
    void OnResourceShallowSaved(const ea::string& resourceName) override;

private:
    BuildDashboardResource& GetDashboard();
    const BuildDashboardResource& GetDashboard() const;
    JSONValue CaptureDashboard() const;
    void CommitDashboardEdit(const JSONValue& before, const ea::string& status);
    void ResetTemplate();
    void AddTask();
    void RemoveSelectedTask();
    void ValidateGraph();
    void RenderTasks(BuildDashboardResource& dashboard);
    void RenderTaskInspector(BuildDashboardResource& dashboard);
    void RenderBuildOrder(const BuildDashboardResource& dashboard);

    SharedPtr<BuildDashboardResource> resource_;
    BuildDashboardResource preview_;
    ea::string selectedTaskKey_;
    ea::string status_;
    ea::string validationError_;
    ea::string newTaskKey_;
    ea::string dependencyInput_;
    unsigned newTaskKind_{};
};

} // namespace Urho3D
