#pragma once

#include "../../Foundation/Shared/InspectorSource.h"
#include "../../Project/ProjectRequest.h"

#include <Urho3D/Core/Object.h>

#include <EASTL/vector.h>

namespace Urho3D
{

class Component;
class EditorTab;
class InspectorTab;
class NavigationMesh;
class Project;
class Scene;
class Terrain;
class TileMap2D;

/// Register production-oriented scene component controls in the Inspector.
void Foundation_SceneProductionInspector(Context* context, InspectorTab* inspectorTab);

/// Specialized inspector addon for terrain, tilemap and navigation production workflows.
///
/// The generic reflected inspector remains the source of truth for serialized attributes.
/// This addon adds production metrics and runtime-safe commands on top of that inspector.
class SceneProductionInspector : public Object, public InspectorSource
{
    URHO3D_OBJECT(SceneProductionInspector, Object);

public:
    explicit SceneProductionInspector(Project* project);

    EditorTab* GetOwnerTab() override { return inspectedTab_; }
    bool IsUndoSupported() override { return true; }
    void RenderContent() override;
    void RenderContextMenuItems() override;
    void RenderMenu() override;
    void ApplyHotkeys(HotkeyManager* hotkeyManager) override;

private:
    void OnProjectRequest(RefCounted* senderTab, ProjectRequest* request);
    void InspectObjects(const InspectNodeComponentRequest& request, EditorTab* inspectedTab);
    void RenderTerrain(Terrain* terrain);
    void RenderTileMap(TileMap2D* tileMap);
    void RenderNavigation(NavigationMesh* navigationMesh);
    void RenderNavigationDiagnostics(NavigationMesh* navigationMesh);

    WeakPtr<Project> project_;
    WeakPtr<EditorTab> inspectedTab_;
    WeakPtr<Scene> scene_;
    ea::vector<WeakPtr<Component>> components_;
    ea::string status_;
    bool hasSupportedSelection_{};
};

} // namespace Urho3D
