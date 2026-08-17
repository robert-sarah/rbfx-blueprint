// SPDX-License-Identifier: MIT

#pragma once

#include "../Project/ResourceEditorTab.h"

#include <Urho3D/Particles/VFXGraphResource.h>

namespace Urho3D
{

/// Register the VFX graph editor tab in a project.
void Foundation_VFXGraphTab(Context* context, Project* project);

/// Production-oriented VFX graph editor with deterministic resource snapshots and bounded preview simulation.
class VFXGraphTab : public ResourceEditorTab
{
    URHO3D_OBJECT(VFXGraphTab, ResourceEditorTab);

public:
    explicit VFXGraphTab(Context* context);

    /// Implement ResourceEditorTab.
    /// @{
    void RenderContent() override;
    void RenderToolbar() override;
    void RenderContextMenuItems() override;
    bool CanOpenResource(const ResourceFileDescriptor& desc) override;
    bool SupportMultipleResources() override { return false; }
    ea::string GetResourceTitle() override { return "VFX Graph"; }
    bool IsUndoSupported() override { return true; }
    /// @}

    /// Apply a complete graph snapshot for undo/redo.
    void ApplyGraphSnapshot(const JSONValue& snapshot);

protected:
    /// Resource lifecycle callbacks.
    /// @{
    void OnResourceLoaded(const ea::string& resourceName) override;
    void OnResourceUnloaded(const ea::string& resourceName) override;
    void OnActiveResourceChanged(const ea::string& oldResourceName, const ea::string& newResourceName) override;
    void OnResourceSaved(const ea::string& resourceName) override;
    void OnResourceShallowSaved(const ea::string& resourceName) override;
    /// @}

private:
    VFXGraph& GetGraph();
    const VFXGraph& GetGraph() const;
    JSONValue CaptureGraph() const;
    void CommitGraphEdit(const JSONValue& before, const ea::string& status);
    void ResetDemoGraph();
    void ValidateGraph();
    void TogglePreview();
    void RenderNodeList(VFXGraph& graph);
    void RenderNodeInspector(VFXGraph& graph);
    void RenderSimulationControls(VFXGraph& graph);
    void RenderPreviewStats(const VFXGraph& graph);

    SharedPtr<VFXGraphResource> resource_;
    VFXGraph previewGraph_;
    ea::string status_;
    ea::string validationError_;
    unsigned selectedNodeId_{};
    unsigned newNodeType_{};
};

} // namespace Urho3D
