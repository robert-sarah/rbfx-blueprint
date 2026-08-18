#include "../InspectorTab/SceneProductionInspector.h"

#include "../../Project/EditorTab.h"
#include "../InspectorTab.h"

#include <Urho3D/Graphics/Terrain.h>
#include <Urho3D/Navigation/DynamicNavigationMesh.h>
#include <Urho3D/Navigation/NavigationMesh.h>
#include <Urho3D/Urho2D/TileMap2D.h>
#include <Urho3D/Urho2D/TileMapDefs2D.h>

#include <IconFontCppHeaders/IconsFontAwesome6.h>

namespace Urho3D
{

void Foundation_SceneProductionInspector(Context* context, InspectorTab* inspectorTab)
{
    inspectorTab->RegisterAddon<SceneProductionInspector>(inspectorTab->GetProject());
}

SceneProductionInspector::SceneProductionInspector(Project* project)
    : Object(project->GetContext())
    , project_(project)
{
    project_->OnRequest.SubscribeWithSender(this, &SceneProductionInspector::OnProjectRequest);
}

void SceneProductionInspector::OnProjectRequest(RefCounted* senderTab, ProjectRequest* request)
{
    auto inspectedTab = dynamic_cast<EditorTab*>(senderTab);
    if (!inspectedTab)
        return;

    auto inspectRequest = dynamic_cast<InspectNodeComponentRequest*>(request);
    if (!inspectRequest || inspectRequest->IsEmpty())
        return;

    bool supported = false;
    for (const WeakPtr<Component>& weakComponent : inspectRequest->GetComponents())
    {
        Component* component = weakComponent;
        if (dynamic_cast<Terrain*>(component) || dynamic_cast<TileMap2D*>(component)
            || dynamic_cast<NavigationMesh*>(component))
        {
            supported = true;
            break;
        }
    }
    if (!supported)
        return;

    request->QueueProcessCallback([this, inspectRequest, inspectedTab]()
    {
        InspectObjects(*inspectRequest, inspectedTab);
        OnActivated(this);
    });
}

void SceneProductionInspector::InspectObjects(const InspectNodeComponentRequest& request, EditorTab* inspectedTab)
{
    inspectedTab_ = inspectedTab;
    scene_ = request.GetCommonScene();
    components_ = request.GetComponents();
    hasSupportedSelection_ = false;
    status_.clear();

    for (const WeakPtr<Component>& weakComponent : components_)
    {
        Component* component = weakComponent;
        if (dynamic_cast<Terrain*>(component) || dynamic_cast<TileMap2D*>(component)
            || dynamic_cast<NavigationMesh*>(component))
        {
            hasSupportedSelection_ = true;
            break;
        }
    }
}

void SceneProductionInspector::RenderContent()
{
    if (!hasSupportedSelection_)
        return;

    ui::TextUnformatted("Scene Production Tools");
    ui::TextDisabled("Specialized controls complement the reflected component inspector.");
    ui::Separator();

    ea::unordered_set<Component*> rendered;
    for (const WeakPtr<Component>& weakComponent : components_)
    {
        Component* component = weakComponent;
        if (!component || !rendered.insert(component).second)
            continue;

        if (auto terrain = dynamic_cast<Terrain*>(component))
            RenderTerrain(terrain);
        else if (auto tileMap = dynamic_cast<TileMap2D*>(component))
            RenderTileMap(tileMap);
        else if (auto navigationMesh = dynamic_cast<NavigationMesh*>(component))
            RenderNavigation(navigationMesh);
    }

    if (!status_.empty())
    {
        ui::Separator();
        ui::TextWrapped("%s", status_.c_str());
    }
}

void SceneProductionInspector::RenderTerrain(Terrain* terrain)
{
    if (!terrain || !ui::CollapsingHeader("Terrain Production", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    const IntVector2& vertices = terrain->GetNumVertices();
    const IntVector2& patches = terrain->GetNumPatches();
    const Vector3& spacing = terrain->GetSpacing();
    ui::Text("Heightmap vertices: %d x %d", vertices.x_, vertices.y_);
    ui::Text("Patches: %d x %d", patches.x_, patches.y_);
    ui::Text("Patch size: %d quads", terrain->GetPatchSize());
    ui::Text("LOD levels: %u, bias: %.2f", terrain->GetMaxLodLevels(), terrain->GetLodBias());
    ui::Text("Spacing: %.3f, %.3f, %.3f", spacing.x_, spacing.y_, spacing.z_);
    ui::Text("Draw distance: %.1f", terrain->GetDrawDistance());

    if (ui::Button(ICON_FA_ARROWS_ROTATE " Apply Heightmap##TerrainProduction"))
    {
        terrain->ApplyHeightMap();
        status_ = "Terrain heightmap changes applied to patch geometry.";
    }
    if (ui::IsItemHovered())
        ui::SetTooltip("Rebuild terrain patch geometry from the current heightmap.");
}

void SceneProductionInspector::RenderTileMap(TileMap2D* tileMap)
{
    if (!tileMap || !ui::CollapsingHeader("TileMap2D Production", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    const TileMapInfo2D& info = tileMap->GetInfo();
    ui::Text("Map dimensions: %d x %d tiles", info.width_, info.height_);
    ui::Text("Tile size: %.2f x %.2f", info.tileWidth_, info.tileHeight_);
    ui::Text("Layers: %u", tileMap->GetNumLayers());
    ui::Text("Orientation: %s", info.orientation_ == O_ORTHOGONAL ? "Orthogonal"
            : info.orientation_ == O_ISOMETRIC ? "Isometric"
            : info.orientation_ == O_STAGGERED ? "Staggered" : "Hexagonal");

    if (ui::Button(ICON_FA_EYE " Refresh TileMap Debug Geometry##TileMapProduction"))
    {
        tileMap->DrawDebugGeometry();
        status_ = "TileMap2D debug geometry refreshed from the imported TMX data.";
    }
    if (ui::IsItemHovered())
        ui::SetTooltip("Refresh the runtime-generated layer geometry without changing the TMX asset.");
}

void SceneProductionInspector::RenderNavigation(NavigationMesh* navigationMesh)
{
    if (!navigationMesh || !ui::CollapsingHeader("Navigation Mesh Production", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    ui::Text("Mesh: %s", navigationMesh->GetMeshName().c_str());
    ui::Text("Tiles: %u / max %d", static_cast<unsigned>(navigationMesh->GetAllTileIndices().size()),
        navigationMesh->GetMaxTiles());
    ui::Text("Tile size: %d cells", navigationMesh->GetTileSize());
    ui::Text("Cell: %.3f x %.3f", navigationMesh->GetCellSize(), navigationMesh->GetCellHeight());
    ui::Text("Agent: height %.2f, radius %.2f, climb %.2f, slope %.1f",
        navigationMesh->GetAgentHeight(), navigationMesh->GetAgentRadius(), navigationMesh->GetAgentMaxClimb(),
        navigationMesh->GetAgentMaxSlope());
    ui::Text("Partition: %s", navigationMesh->GetPartitionType() == NAVMESH_PARTITION_WATERSHED
            ? "Watershed" : "Monotone");
    RenderNavigationDiagnostics(navigationMesh);

    if (ui::Button(ICON_FA_HARD_DRIVE " Allocate##NavigationProduction"))
    {
        status_ = navigationMesh->Allocate() ? "Navigation mesh storage allocated." : "Navigation mesh allocation failed.";
    }
    ui::SameLine();
    if (ui::Button(ICON_FA_ROTATE " Rebuild##NavigationProduction"))
    {
        status_ = navigationMesh->Rebuild() ? "Navigation mesh rebuilt from scene geometry." : "Navigation mesh rebuild failed.";
    }
    ui::SameLine();
    if (ui::Button(ICON_FA_TRASH_CAN " Clear##NavigationProduction"))
    {
        navigationMesh->Clear();
        status_ = "Navigation mesh tile data cleared.";
    }

    if (auto dynamicNavigationMesh = dynamic_cast<DynamicNavigationMesh*>(navigationMesh))
    {
        ui::Separator();
        ui::Text("Dynamic navmesh: max obstacles %d, max layers %d, draw obstacles %s",
            dynamicNavigationMesh->GetMaxObstacles(), dynamicNavigationMesh->GetMaxLayers(),
            dynamicNavigationMesh->GetDrawObstacles() ? "on" : "off");
    }
}

void SceneProductionInspector::RenderNavigationDiagnostics(NavigationMesh* navigationMesh)
{
    if (!navigationMesh->IsInitialized())
    {
        ui::TextDisabled("Navigation data is not initialized.");
        return;
    }

    const auto tileIndices = navigationMesh->GetAllTileIndices();
    ui::Text("Runtime data: initialized, %u tiles available", static_cast<unsigned>(tileIndices.size()));
}

void SceneProductionInspector::RenderContextMenuItems()
{
}

void SceneProductionInspector::RenderMenu()
{
    if (inspectedTab_)
        inspectedTab_->RenderMenu();
}

void SceneProductionInspector::ApplyHotkeys(HotkeyManager* hotkeyManager)
{
    (void)hotkeyManager;
}

} // namespace Urho3D
