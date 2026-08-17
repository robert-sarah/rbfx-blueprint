// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include "../Project/ResourceEditorTab.h"

#include <Urho3D/Shader/ShaderGraphResource.h>

namespace Urho3D
{

/// Register the material/shader graph editor tab in a project.
void Foundation_ShaderGraphTab(Context* context, Project* project);

/// Production-oriented material graph editor with validation, generated shader preview and resource undo.
class ShaderGraphTab : public ResourceEditorTab
{
    URHO3D_OBJECT(ShaderGraphTab, ResourceEditorTab);

public:
    explicit ShaderGraphTab(Context* context);

    /// Implement ResourceEditorTab.
    /// @{
    void RenderContent() override;
    void RenderToolbar() override;
    void RenderContextMenuItems() override;
    bool CanOpenResource(const ResourceFileDescriptor& desc) override;
    bool SupportMultipleResources() override { return false; }
    ea::string GetResourceTitle() override { return "Shader Graph"; }
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
    ShaderGraph& GetGraph();
    const ShaderGraph& GetGraph() const;
    JSONValue CaptureGraph() const;
    void CommitGraphEdit(const JSONValue& before, const ea::string& status);
    void ResetDemoGraph();
    void ValidateGraph();
    void GenerateShader(ShaderGraphLanguage language);
    void RenderNodeList(ShaderGraph& graph);
    void RenderNodeInspector(ShaderGraph& graph);
    void RenderConnections(const ShaderGraph& graph);
    void RenderGeneratedSource();

    SharedPtr<ShaderGraphResource> resource_;
    ShaderGraph previewGraph_;
    ea::string status_;
    ea::string validationError_;
    ea::string generatedSource_;
    ShaderGraphLanguage generatedLanguage_{ShaderGraphLanguage::GLSL};
    unsigned selectedNodeId_{};
    unsigned newNodeKind_{};
    unsigned newNodeType_{};
};

} // namespace Urho3D
