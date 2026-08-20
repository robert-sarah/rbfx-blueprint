//
// Copyright (c) 2017-2020 the rbfx project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include "../Foundation/SceneViewTab.h"

#include "../Core/CommonEditorActions.h"
#include "../Core/IniHelpers.h"
#include "../Project/CreateComponentMenu.h"

#include <Urho3D/Engine/Engine.h>
#include <Urho3D/Engine/EngineDefs.h>
#include <Urho3D/Engine/EngineEvents.h>
#include <Urho3D/Graphics/Camera.h>
#include <Urho3D/Graphics/DebugRenderer.h>
#include <Urho3D/Graphics/Texture2D.h>
#include <Urho3D/IO/ArchiveSerialization.h>
#include <Urho3D/IO/FileSystem.h>
#include <Urho3D/IO/Log.h>
#include <Urho3D/Input/Input.h>
#include <Urho3D/Plugins/PluginManager.h>
#include <Urho3D/Resource/JSONFile.h>
#include <Urho3D/Resource/ResourceCache.h>
#include <Urho3D/Resource/ResourceEvents.h>
#include <Urho3D/Scene/PrefabResource.h>
#include <Urho3D/Scene/Scene.h>
#include <Urho3D/SystemUI/NodeInspectorWidget.h>
#include <Urho3D/SystemUI/Widgets.h>

#include <IconFontCppHeaders/IconsFontAwesome6.h>

#include <algorithm>
#include <cmath>

namespace Urho3D
{

namespace
{

const auto Hotkey_RewindSimulation = EditorHotkey{"SceneViewTab.RewindSimulation"}.Press(KEY_UNKNOWN);
const auto Hotkey_TogglePaused = EditorHotkey{"SceneViewTab.TogglePaused"}.Press(KEY_PAUSE).MaybeMouse();

const auto Hotkey_Cut = EditorHotkey{"SceneViewTab.Cut"}.Ctrl().Press(KEY_X);
const auto Hotkey_Copy = EditorHotkey{"SceneViewTab.Copy"}.Ctrl().Press(KEY_C);
const auto Hotkey_Paste = EditorHotkey{"SceneViewTab.Paste"}.Ctrl().Press(KEY_V);
const auto Hotkey_PasteInto = EditorHotkey{"SceneViewTab.PasteInto"}.Ctrl().Shift().Press(KEY_V);
const auto Hotkey_Delete = EditorHotkey{"SceneViewTab.Delete"}.Press(KEY_DELETE);
const auto Hotkey_Duplicate = EditorHotkey{"SceneViewTab.Duplicate"}.Ctrl().Press(KEY_D);

const auto Hotkey_Focus = EditorHotkey{"SceneViewTab.Focus"}.Press(KEY_F);
const auto Hotkey_MoveToLatest = EditorHotkey{"SceneViewTab.MoveToLatest"};
const auto Hotkey_MovePositionToLatest = EditorHotkey{"SceneViewTab.MovePositionToLatest"};
const auto Hotkey_MoveRotationToLatest = EditorHotkey{"SceneViewTab.MoveRotationToLatest"};
const auto Hotkey_MoveScaleToLatest = EditorHotkey{"SceneViewTab.MoveScaleToLatest"};
const auto Hotkey_MakePersistent = EditorHotkey{"SceneViewTab.MakePersistent"};

const auto Hotkey_CreateSiblingNode = EditorHotkey{"SceneViewTab.CreateSiblingNode"}.Ctrl().Press(KEY_N);
const auto Hotkey_CreateChildNode = EditorHotkey{"SceneViewTab.CreateChildNode"}.Ctrl().Shift().Press(KEY_N);

void SetSceneNextIds(Scene* scene, unsigned nextNodeId, unsigned nextComponentId)
{
    scene->SetAttribute("Next Node ID", nextNodeId);
    scene->SetAttribute("Next Component ID", nextComponentId);
}

void RecalculateSceneNextIds(Scene* scene)
{
    unsigned nextNodeId = 0;
    unsigned nextComponentId = 0;

    const auto nodeCallback = [&](Node* node)
    {
        if (node->IsTemporary())
            return false;

        nextNodeId = ea::max(nextNodeId, node->GetID() + 1);
        return true;
    };

    const auto componentCallback = [&](Component* component)
    {
        if (!component->IsTemporary())
            nextComponentId = ea::max(nextComponentId, component->GetID() + 1);
    };

    scene->TraverseDepthFirst(nodeCallback, componentCallback);
    SetSceneNextIds(scene, nextNodeId, nextComponentId);
}

}

void SerializeValue(Archive& archive, const char* name, SceneViewPage& page, const SceneViewTab* owner)
{
    auto block = archive.OpenUnorderedBlock(name);

    {
        PackedSceneSelection selection;
        if (!archive.IsInput())
            page.selection_.Save(selection);

        SerializeOptionalValue(archive, "Selection", selection, AlwaysSerialize{});

        if (archive.IsInput())
            page.selection_.Load(page.scene_, selection);
    }

    {
        auto addonsBlock = archive.OpenUnorderedBlock("Addons");
        for (const SceneViewAddon* addon : owner->GetAddonsByName())
        {
            ea::any& state = page.GetAddonData(*addon);
            SerializeOptionalValue(archive, addon->GetUniqueName().c_str(), state, AlwaysSerialize{},
                [&](Archive& archive, const char* name, ea::any& value)
            {
                addon->SerializePageState(archive, name, value);
            });
        }
    }
}

void Foundation_SceneViewTab(Context* context, Project* project)
{
    project->AddTab(MakeShared<SceneViewTab>(context));

    if (!context->IsReflected<SceneResourceForEditor>())
        context->AddFactoryReflection<SceneResourceForEditor>();
}

SceneViewPane::SceneViewPane(Scene* scene, SceneViewPaneType type, const ea::string& label)
    : type_(type)
    , label_(label)
    , renderer_(MakeShared<SceneRendererToTexture>(scene))
{
    Camera* camera = renderer_->GetCamera();
    Node* cameraNode = renderer_->GetCameraNode();
    camera->SetOrthographic(type != SceneViewPaneType::Perspective);
    if (type == SceneViewPaneType::Perspective)
    {
        camera->SetFov(45.0f);
        cameraNode->SetPosition(Vector3{0.0f, 3.0f, -10.0f});
        cameraNode->SetRotation(Quaternion{15.0f, 0.0f, 0.0f});
    }
    else
    {
        camera->SetOrthoSize(orthoSize_);
        if (type == SceneViewPaneType::Top)
        {
            cameraNode->SetPosition(Vector3{0.0f, 100.0f, 0.0f});
            cameraNode->SetRotation(Quaternion{90.0f, 0.0f, 0.0f});
        }
        else if (type == SceneViewPaneType::Front)
        {
            cameraNode->SetPosition(Vector3{0.0f, 0.0f, -100.0f});
            cameraNode->SetRotation(Quaternion::IDENTITY);
        }
        else
        {
            cameraNode->SetPosition(Vector3{100.0f, 0.0f, 0.0f});
            cameraNode->SetRotation(Quaternion{0.0f, -90.0f, 0.0f});
        }
    }
    renderer_->SetActive(false);
}

SceneViewPage::SceneViewPage(SceneResource* resource)
    : Object(resource->GetContext())
    , resource_(resource)
    , scene_(resource->GetScene())
    , renderer_(MakeShared<SceneRendererToTexture>(scene_))
    , cfgFileName_(resource_->GetAbsoluteFileName() + ".user.json")
{
    scene_->SetFileName(resource_->GetAbsoluteFileName());
    scene_->SetUpdateEnabled(false);
    multiViewports_.emplace_back(scene_, SceneViewPaneType::Perspective, "Perspective");
    multiViewports_.emplace_back(scene_, SceneViewPaneType::Top, "Top");
    multiViewports_.emplace_back(scene_, SceneViewPaneType::Front, "Front");
    multiViewports_.emplace_back(scene_, SceneViewPaneType::Right, "Right");
}

SceneViewPage::~SceneViewPage()
{
}

void SceneViewPage::SerializeInBlock(Archive& archive)
{
    SerializeOptionalValue(archive, "Scene2DGridEnabled", scene2DGridEnabled_, AlwaysSerialize{});
    SerializeOptionalValue(archive, "Scene2DSnapEnabled", scene2DSnapEnabled_, AlwaysSerialize{});
    SerializeOptionalValue(archive, "Scene2DGridSpacing", scene2DGridSpacing_, AlwaysSerialize{});
    SerializeOptionalValue(archive, "Scene2DSnapSpacing", scene2DSnapSpacing_, AlwaysSerialize{});

    if (archive.IsInput())
    {
        scene2DGridSpacing_ = Clamp(scene2DGridSpacing_, 0.1f, 1000.0f);
        scene2DSnapSpacing_ = Clamp(scene2DSnapSpacing_, 0.1f, 1000.0f);
    }
}

ea::any& SceneViewPage::GetAddonData(const SceneViewAddon& addon)
{
    auto& [addonPtr, addonData] = addonData_[addon.GetUniqueName()];
    if (addonPtr != &addon)
    {
        addonPtr = &addon;
        addonData = ea::any{};
    }
    return addonData;
}

bool SceneViewPage::IsSimulationActive() const
{
    return currentSimulationAction_ && !currentSimulationAction_->IsComplete();
}

void SceneViewPage::StartSimulation(SceneViewTab* owner)
{
    currentSimulationAction_ = owner->PushAction<SimulateSceneAction>(this);
}

void SceneViewPage::BeginSelection()
{
    selection_.Update();
    selection_.Save(oldSelection_);
}

void SceneViewPage::EndSelection(SceneViewTab* owner)
{
    selection_.Save(newSelection_);
    if (oldSelection_ != newSelection_)
        owner->PushAction<ChangeSceneSelectionAction>(this, oldSelection_, newSelection_);
}

SceneViewAddon::SceneViewAddon(SceneViewTab* owner)
    : Object(owner->GetContext())
    , owner_(owner)
{
}

SceneViewAddon::~SceneViewAddon()
{
}

void SceneViewAddon::ApplyHotkeys(HotkeyManager* hotkeyManager)
{
    hotkeyManager->InvokeFor(this);
}

void SceneViewAddon::SerializePageState(Archive& archive, const char* name, ea::any& stateWrapped) const
{
    // Just open empty block
    EmptySerializableObject placeholder;
    SerializeOptionalValue(archive, name, placeholder, AlwaysSerialize{});
}

bool SceneViewTab::ByInputPriority::operator()(const SharedPtr<SceneViewAddon>& lhs, const SharedPtr<SceneViewAddon>& rhs) const
{
    return lhs->GetInputPriority() > rhs->GetInputPriority();
}

bool SceneViewTab::ByToolbarPriority::operator()(const SharedPtr<SceneViewAddon>& lhs, const SharedPtr<SceneViewAddon>& rhs) const
{
    return lhs->GetToolbarPriority() < rhs->GetToolbarPriority();
}

bool SceneViewTab::ByName::operator()(const SharedPtr<SceneViewAddon>& lhs, const SharedPtr<SceneViewAddon>& rhs) const
{
    return lhs->GetUniqueName() < rhs->GetUniqueName();
}

SceneViewTab::SceneViewTab(Context* context)
    : ResourceEditorTab(context, "Scene", "9f4f7432-dd60-4c83-aecd-2f6cf69d3549",
        EditorTabFlag::NoContentPadding | EditorTabFlag::OpenByDefault | EditorTabFlag::FocusOnStart,
        EditorTabPlacement::DockCenter)
{
    BindHotkey(Hotkey_RewindSimulation, &SceneViewTab::RewindSimulation);
    BindHotkey(Hotkey_TogglePaused, &SceneViewTab::ToggleSimulationPaused);
    BindHotkey(Hotkey_Cut, &SceneViewTab::CutSelection);
    BindHotkey(Hotkey_Copy, &SceneViewTab::CopySelection);
    BindHotkey(Hotkey_Paste, &SceneViewTab::PasteNextToSelection);
    BindHotkey(Hotkey_PasteInto, &SceneViewTab::PasteIntoSelection);
    BindHotkey(Hotkey_Delete, &SceneViewTab::DeleteSelection);
    BindHotkey(Hotkey_Duplicate, &SceneViewTab::DuplicateSelection);
    BindHotkey(Hotkey_Focus, &SceneViewTab::FocusSelection);
    BindHotkey(Hotkey_MoveToLatest, &SceneViewTab::MoveSelectionToLatest);
    BindHotkey(Hotkey_MovePositionToLatest, &SceneViewTab::MoveSelectionPositionToLatest);
    BindHotkey(Hotkey_MoveRotationToLatest, &SceneViewTab::MoveSelectionRotationToLatest);
    BindHotkey(Hotkey_MoveScaleToLatest, &SceneViewTab::MoveSelectionScaleToLatest);
    BindHotkey(Hotkey_MakePersistent, &SceneViewTab::MakePersistent);
    BindHotkey(Hotkey_CreateSiblingNode, &SceneViewTab::CreateNodeNextToSelection);
    BindHotkey(Hotkey_CreateChildNode, &SceneViewTab::CreateNodeInSelection);

    SubscribeToEvent(E_BEGINPLUGINRELOAD, &SceneViewTab::BeginPluginReload);
    SubscribeToEvent(E_ENDPLUGINRELOAD, &SceneViewTab::EndPluginReload);
}

SceneViewTab::~SceneViewTab()
{
}

void SceneViewTab::RegisterAddon(const SharedPtr<SceneViewAddon>& addon)
{
    addons_.push_back(addon);
    addonsByInputPriority_.insert(addon);
    addonsByToolbarPriority_.insert(addon);
    addonsByName_.insert(addon);
}

void SceneViewTab::SetupPluginContext()
{
    SceneViewPage* activePage = GetActivePage();

    auto engine = GetSubsystem<Engine>();
    if (activePage)
    {
        engine->SetParameter(Param_SceneName, GetActiveResourceName());
        engine->SetParameter(Param_ScenePosition, activePage->renderer_->GetCameraPosition());
        engine->SetParameter(Param_SceneRotation, activePage->renderer_->GetCameraRotation());
    }
    else
    {
        engine->SetParameter(Param_SceneName, Variant::EMPTY);
        engine->SetParameter(Param_ScenePosition, Variant::EMPTY);
        engine->SetParameter(Param_SceneRotation, Variant::EMPTY);
    }
}

void SceneViewTab::RenderEditMenu(Scene* scene, SceneSelection& selection)
{
    const bool hasNodeSelection = !selection.GetNodes().empty();
    const bool hasSelection = hasNodeSelection || !selection.GetComponents().empty();
    const bool hasClipboard = clipboard_.HasNodesOrComponents();

    if (ui::MenuItem("Cut", GetHotkeyLabel(Hotkey_Cut).c_str(), false, hasSelection))
        CutSelection(selection);
    if (ui::MenuItem("Copy", GetHotkeyLabel(Hotkey_Copy).c_str(), false, hasSelection))
        CopySelection(selection);
    if (ui::MenuItem("Paste", GetHotkeyLabel(Hotkey_Paste).c_str(), false, hasClipboard))
        PasteNextToSelection(scene, selection);
    if (ui::MenuItem("Paste Into", GetHotkeyLabel(Hotkey_PasteInto).c_str(), false, hasClipboard))
        PasteIntoSelection(scene, selection);
    if (ui::MenuItem("Delete", GetHotkeyLabel(Hotkey_Delete).c_str(), false, hasSelection))
        DeleteSelection(selection);
    if (ui::MenuItem("Duplicate", GetHotkeyLabel(Hotkey_Duplicate).c_str(), false, hasSelection))
        DuplicateSelection(selection);

    ui::Separator();

    if (ui::MenuItem("Focus", GetHotkeyLabel(Hotkey_Focus).c_str(), false, hasSelection))
        FocusSelection(selection);

    if (hasNodeSelection)
    {
        if (ui::MenuItem("Move to Latest", GetHotkeyLabel(Hotkey_MoveToLatest).c_str(), false))
            MoveSelectionToLatest(selection);
        if (ui::BeginMenu("Move Attribute to Latest..."))
        {
            if (ui::MenuItem("Position", GetHotkeyLabel(Hotkey_MovePositionToLatest).c_str(), false))
                MoveSelectionPositionToLatest(selection);
            if (ui::MenuItem("Rotation", GetHotkeyLabel(Hotkey_MoveRotationToLatest).c_str(), false))
                MoveSelectionRotationToLatest(selection);
            if (ui::MenuItem("Scale", GetHotkeyLabel(Hotkey_MoveScaleToLatest).c_str(), false))
                MoveSelectionScaleToLatest(selection);
            ui::EndMenu();
        }
    }

    if (ui::MenuItem("Make Persistent", GetHotkeyLabel(Hotkey_MakePersistent).c_str(), false, hasSelection))
        MakePersistent(selection);

    if (SceneViewPage* activePage = GetActivePage())
    {
        ui::Separator();
        OnSelectionEditMenu(this, *activePage, scene, selection);
    }

    const bool hasComponentSelection = !selection.GetComponents().empty();
    if (hasComponentSelection)
    {
        ui::Separator();

        if (ui::BeginMenu("Replace Component"))
        {
            if (const auto componentType = RenderCreateComponentMenu(context_))
                ReplaceSelectedComponents(selection, componentType->GetTypeNameHash());

            ui::EndMenu();
        }
    }
}

void SceneViewTab::RenderCreateMenu(Scene* scene, SceneSelection& selection)
{
    if (ui::MenuItem("Create Node", GetHotkeyLabel(Hotkey_CreateSiblingNode).c_str()))
        CreateNodeNextToSelection(scene, selection);

    if (ui::MenuItem("Create Child Node", GetHotkeyLabel(Hotkey_CreateChildNode).c_str()))
        CreateNodeInSelection(scene, selection);

    ui::MenuItem("Create Component:", nullptr, false, false);
    ui::Indent();
    if (const auto componentType = RenderCreateComponentMenu(context_))
        CreateComponentInSelection(scene, selection, componentType->GetTypeNameHash());
    ui::Unindent();
}

void SceneViewTab::ResumeSimulation()
{
    SceneViewPage* activePage = GetActivePage();
    if (!activePage)
        return;

    if (!activePage->IsSimulationActive())
        activePage->StartSimulation(this);
    activePage->scene_->SetUpdateEnabled(true);
}

void SceneViewTab::PauseSimulation()
{
    SceneViewPage* activePage = GetActivePage();
    if (!activePage)
        return;

    if (activePage->IsSimulationActive())
        activePage->currentSimulationAction_->Complete(true);
    activePage->scene_->SetUpdateEnabled(false);
}

void SceneViewTab::ToggleSimulationPaused()
{
    SceneViewPage* activePage = GetActivePage();
    if (!activePage)
        return;

    if (activePage->scene_->IsUpdateEnabled())
        PauseSimulation();
    else
        ResumeSimulation();
}

void SceneViewTab::RewindSimulation()
{
    SceneViewPage* activePage = GetActivePage();
    if (!activePage || !activePage->IsSimulationActive())
        return;

    // Simulation is stored as EditorAction, so we can just undo it
    UndoManager* undoManager = GetUndoManager();
    undoManager->Undo();
}

void SceneViewTab::CompactObjectIds()
{
    SceneViewPage* activePage = GetActivePage();
    if (!activePage)
        return;

    // Preserve and clear selection
    PackedSceneSelection oldSelectionData;
    activePage->selection_.Save(oldSelectionData);
    PushAction(MakeShared<ChangeSceneSelectionAction>(activePage, oldSelectionData, PackedSceneSelection{}));
    activePage->selection_.Clear();

    // Preserve and remap scene;
    const PackedSceneData oldSceneData = PackedSceneData::FromScene(activePage->scene_);

    SetSceneNextIds(activePage->scene_, 0, 0);
    PackedSceneData sceneData = PackedSceneData::FromScene(activePage->scene_);
    sceneData.ToScene(activePage->scene_, PrefabLoadFlag::DiscardIds);

    PushAction(MakeShared<ChangeSceneAction>(activePage->scene_, oldSceneData));
}

void SceneViewTab::CutSelection(SceneSelection& selection)
{
    CopySelection(selection);
    DeleteSelection(selection);
}

void SceneViewTab::CopySelection(SceneSelection& selection)
{
    const auto& selectedNodes = selection.GetNodes();
    const auto& selectedComponents = selection.GetComponents();

    if (!selectedNodes.empty())
        clipboard_ = PackedNodeComponentData::FromNodes(selectedNodes.begin(), selectedNodes.end());
    else if (!selectedComponents.empty())
        clipboard_ = PackedNodeComponentData::FromComponents(selectedComponents.begin(), selectedComponents.end());
}

void SceneViewTab::PasteNextToSelection(Scene* scene, SceneSelection& selection)
{
    if (clipboard_.HasNodes())
    {
        Node* siblingNode = selection.GetActiveNodeOrScene();
        Node* parentNode = siblingNode && siblingNode->GetParent() ? siblingNode->GetParent() : scene;

        selection.Clear();
        for (const PackedNodeData& packedNode : clipboard_.GetNodes())
        {
            const CreateNodeActionBuilder builder{scene, packedNode.GetEffectiveScopeHint()};

            Node* newNode = packedNode.SpawnCopy(parentNode);
            selection.SetSelected(newNode, true);

            PushAction(builder.Build(newNode));
        }
    }
    else if (clipboard_.HasComponents())
    {
        PasteIntoSelection(scene, selection);
    }
}

void SceneViewTab::PasteIntoSelection(Scene* scene, SceneSelection& selection)
{
    // Copy because selection changes during paste
    auto parentNodes = selection.GetEffectiveNodesAndScenes();
    if (parentNodes.empty())
        parentNodes.emplace(scene);

    if (clipboard_.HasNodes())
    {
        selection.Clear();
        for (Node* selectedNode : parentNodes)
        {
            for (const PackedNodeData& packedNode : clipboard_.GetNodes())
            {
                const CreateNodeActionBuilder builder{scene, packedNode.GetEffectiveScopeHint()};

                Node* newNode = packedNode.SpawnCopy(selectedNode);
                selection.SetSelected(newNode, true);

                PushAction(builder.Build(newNode));
            }
        }
    }
    else if (clipboard_.HasComponents())
    {
        selection.Clear();
        for (Node* selectedNode : parentNodes)
        {
            for (const PackedComponentData& packedComponent : clipboard_.GetComponents())
            {
                const CreateComponentActionBuilder builder(selectedNode, packedComponent.GetType());
                Component* newComponent = packedComponent.SpawnCopy(selectedNode);
                PushAction(builder.Build(newComponent));

                if (componentSelection_)
                    selection.SetSelected(newComponent, true);
                else
                    selection.SetSelected(selectedNode, true);
            }
        }
    }
}

void SceneViewTab::DeleteSelection(SceneSelection& selection)
{
    const auto& selectedNodes = selection.GetNodes();
    const auto& selectedComponents = selection.GetComponents();

    for (Node* node : selectedNodes)
    {
        if (node && node->GetParent() != nullptr)
        {
            const RemoveNodeActionBuilder builder(node);

            node->Remove();

            PushAction(builder.Build());
        }
    }

    for (Component* component : selectedComponents)
    {
        if (component)
        {
            const RemoveComponentActionBuilder builder(component);
            component->Remove();
            PushAction(builder.Build());
        }
    }

    selection.Clear();
}

void SceneViewTab::DuplicateSelection(SceneSelection& selection)
{
    if (!selection.GetNodes().empty())
    {
        // Copy because selection changes during paste
        const auto selectedNodes = selection.GetNodes();
        selection.Clear();

        for (Node* node : selectedNodes)
        {
            Node* parent = node->GetParent();
            URHO3D_ASSERT(parent);
            const auto data = PackedNodeData{node};

            const CreateNodeActionBuilder builder{parent->GetScene(), data.GetEffectiveScopeHint()};

            Node* newNode = data.SpawnCopy(parent);
            selection.SetSelected(newNode, true);

            PushAction(builder.Build(newNode));
        }
    }
    else if (!selection.GetComponents().empty())
    {
        // Copy because selection changes during paste
        const auto selectedComponents = selection.GetComponents();
        selection.Clear();

        for (Component* component : selectedComponents)
        {
            Node* node = component->GetNode();
            URHO3D_ASSERT(node);

            const auto data = PackedComponentData{component};

            const CreateComponentActionBuilder builder(node, data.GetType());
            Component* newComponent = data.SpawnCopy(node);
            PushAction(builder.Build(newComponent));

            if (componentSelection_)
                selection.SetSelected(newComponent, true);
            else
                selection.SetSelected(node, true);
        }
    }
}

void SceneViewTab::CreateNodeNextToSelection(Scene* scene, SceneSelection& selection)
{
    Node* siblingNode = selection.GetActiveNodeOrScene();
    Node* parentNode = siblingNode && siblingNode->GetParent() ? siblingNode->GetParent() : scene;

    const CreateNodeActionBuilder builder{scene, AttributeScopeHint::Attribute};

    Node* newNode = parentNode->CreateChild();
    selection.Clear();
    selection.SetSelected(newNode, true);

    PushAction(builder.Build(newNode));
}

void SceneViewTab::CreateNodeInSelection(Scene* scene, SceneSelection& selection)
{
    // Copy because selection changes during paste
    auto parentNodes = selection.GetEffectiveNodesAndScenes();
    if (parentNodes.empty())
        parentNodes.emplace(scene);

    selection.Clear();
    for (Node* selectedNode : parentNodes)
    {
        const CreateNodeActionBuilder builder{scene, AttributeScopeHint::Attribute};

        Node* newNode = selectedNode->CreateChild();
        selection.SetSelected(newNode, true);

        PushAction(builder.Build(newNode));
    }
}

void SceneViewTab::CreateComponentInSelection(Scene* scene, SceneSelection& selection, StringHash componentType)
{
    // Copy because selection changes during paste
    auto parentNodes = selection.GetEffectiveNodesAndScenes();
    if (parentNodes.empty())
        parentNodes.emplace(scene);

    selection.Clear();
    for (Node* selectedNode : parentNodes)
    {
        const CreateComponentActionBuilder builder(selectedNode, componentType);
        Component* newComponent = selectedNode->CreateComponent(componentType);
        PushAction(builder.Build(newComponent));

        if (componentSelection_)
            selection.SetSelected(newComponent, true);
        else
            selection.SetSelected(selectedNode, true);
    }
}

void SceneViewTab::FocusSelection(SceneSelection& selection)
{
    if (Node* activeNode = selection.GetActiveNode())
    {
        if (SceneViewPage* page = GetPage(activeNode->GetScene()))
            OnLookAt(this, *page, activeNode->GetWorldPosition());
    }
}

void SceneViewTab::MoveSelectionToLatest(SceneSelection& selection)
{
    if (Node* activeNode = selection.GetActiveNode())
    {
        const Matrix3x4 worldTransform = activeNode->GetWorldTransform();

        for (Node* node : selection.GetNodes())
        {
            if (node && node != activeNode)
                node->SetWorldTransform(worldTransform);
        }
    }
}

void SceneViewTab::MoveSelectionPositionToLatest(SceneSelection& selection)
{
    if (Node* activeNode = selection.GetActiveNode())
    {
        const Vector3 worldPosition = activeNode->GetWorldPosition();

        for (Node* node : selection.GetNodes())
        {
            if (node && node != activeNode)
                node->SetWorldPosition(worldPosition);
        }
    }
}

void SceneViewTab::MoveSelectionRotationToLatest(SceneSelection& selection)
{
    if (Node* activeNode = selection.GetActiveNode())
    {
        const Quaternion worldRotation = activeNode->GetWorldRotation();

        for (Node* node : selection.GetNodes())
        {
            if (node && node != activeNode)
                node->SetWorldRotation(worldRotation);
        }
    }
}

void SceneViewTab::MoveSelectionScaleToLatest(SceneSelection& selection)
{
    if (Node* activeNode = selection.GetActiveNode())
    {
        const Vector3 worldScale = activeNode->GetWorldScale();

        for (Node* node : selection.GetNodes())
        {
            if (node && node != activeNode)
                node->SetWorldScale(worldScale);
        }
    }
}

void SceneViewTab::MakePersistent(SceneSelection& selection)
{
    for (Node* node : selection.GetNodes())
    {
        if (node)
            node->SetTemporary(false);
    }

    for (Component* component : selection.GetComponents())
    {
        if (component)
            component->SetTemporary(false);
    }
}

void SceneViewTab::ReplaceSelectedComponents(SceneSelection& selection, StringHash componentType)
{
    // Copy selection because it is modified during replacement
    const auto selectedComponents = selection.GetComponents();
    selection.Clear();

    auto action = MakeShared<CompositeEditorAction>();
    for (Component* oldComponent : selectedComponents)
    {
        if (!oldComponent)
            continue;

        Node* node = oldComponent->GetNode();
        const unsigned componentIndex = node->GetComponentIndex(oldComponent);

        // Create new component
        {
            const CreateComponentActionBuilder createBuilder(node, componentType);
            Component* newComponent = node->CreateComponent(componentType);
            if (!newComponent)
                continue;

            newComponent->CopyAttributes(oldComponent);
            newComponent->ApplyAttributes();
            node->ReorderComponent(newComponent, componentIndex);

            action->AddAction(createBuilder.Build(newComponent));

            selection.SetSelected(newComponent, true);
        }

        // Remove old component
        {
            const RemoveComponentActionBuilder removeBuilder(oldComponent);
            oldComponent->Remove();
            action->AddAction(removeBuilder.Build());
        }
    }

    // Push composite action to undo stack
    PushAction(action);
}

void SceneViewTab::CutSelection()
{
    if (SceneViewPage* activePage = GetActivePage())
        CutSelection(activePage->selection_);
}

void SceneViewTab::CopySelection()
{
    if (SceneViewPage* activePage = GetActivePage())
        CopySelection(activePage->selection_);
}

void SceneViewTab::PasteNextToSelection()
{
    if (SceneViewPage* activePage = GetActivePage())
        PasteNextToSelection(activePage->scene_, activePage->selection_);
}

void SceneViewTab::PasteIntoSelection()
{
    if (SceneViewPage* activePage = GetActivePage())
        PasteIntoSelection(activePage->scene_, activePage->selection_);
}

void SceneViewTab::DeleteSelection()
{
    if (SceneViewPage* activePage = GetActivePage())
        DeleteSelection(activePage->selection_);
}

void SceneViewTab::DuplicateSelection()
{
    if (SceneViewPage* activePage = GetActivePage())
        DuplicateSelection(activePage->selection_);
}

void SceneViewTab::CreateNodeNextToSelection()
{
    if (SceneViewPage* activePage = GetActivePage())
        CreateNodeNextToSelection(activePage->scene_, activePage->selection_);
}

void SceneViewTab::CreateNodeInSelection()
{
    if (SceneViewPage* activePage = GetActivePage())
        CreateNodeInSelection(activePage->scene_, activePage->selection_);
}

void SceneViewTab::FocusSelection()
{
    if (SceneViewPage* activePage = GetActivePage())
        FocusSelection(activePage->selection_);
}

void SceneViewTab::MoveSelectionToLatest()
{
    if (SceneViewPage* activePage = GetActivePage())
        MoveSelectionToLatest(activePage->selection_);
}

void SceneViewTab::MoveSelectionPositionToLatest()
{
    if (SceneViewPage* activePage = GetActivePage())
        MoveSelectionPositionToLatest(activePage->selection_);
}

void SceneViewTab::MoveSelectionRotationToLatest()
{
    if (SceneViewPage* activePage = GetActivePage())
        MoveSelectionRotationToLatest(activePage->selection_);
}

void SceneViewTab::MoveSelectionScaleToLatest()
{
    if (SceneViewPage* activePage = GetActivePage())
        MoveSelectionScaleToLatest(activePage->selection_);
}

void SceneViewTab::MakePersistent()
{
    if (SceneViewPage* activePage = GetActivePage())
        MakePersistent(activePage->selection_);
}

void SceneViewTab::RenderMenu()
{
    if (ui::BeginMenu("Edit"))
    {
        RenderEditMenuItems();

        if (SceneViewPage* activePage = GetActivePage())
        {
            ui::Separator();
            RenderEditMenu(activePage->scene_, activePage->selection_);
        }

        ui::EndMenu();
    }

    if (SceneViewPage* activePage = GetActivePage())
    {
        if (ui::BeginMenu("Create"))
        {
            RenderCreateMenu(activePage->scene_, activePage->selection_);
            ui::EndMenu();
        }
    }
}

void SceneViewTab::Configure2DView(SceneViewPage& page, bool enabled)
{
    Camera* camera = page.renderer_->GetCamera();
    Node* cameraNode = page.renderer_->GetCameraNode();
    if (enabled)
    {
        camera->SetOrthographic(true);
        camera->SetOrthoSize(50.0f);
        cameraNode->SetPosition(Vector3{0.0f, 0.0f, -100.0f});
        cameraNode->SetRotation(Quaternion::IDENTITY);
        status_ = "2D mode: XY canvas, orthographic camera";
    }
    else
    {
        camera->SetOrthographic(false);
        camera->SetFov(45.0f);
        cameraNode->SetPosition(Vector3{0.0f, 3.0f, -10.0f});
        cameraNode->SetRotation(Quaternion{15.0f, 0.0f, 0.0f});
        status_ = "3D mode: perspective camera";
    }
}

void SceneViewTab::RenderScene2DGrid(const SceneViewPage& page) const
{
    if (!scene2DMode_ || !page.scene2DGridEnabled_ || page.contentArea_.min_.x_ >= page.contentArea_.max_.x_
        || page.contentArea_.min_.y_ >= page.contentArea_.max_.y_)
    {
        return;
    }

    Camera* camera = page.renderer_->GetCamera();
    if (!camera || !camera->IsOrthographic())
        return;

    const float spacing = Clamp(page.scene2DGridSpacing_, 0.1f, 1000.0f);
    const Vector2 areaMin = page.contentArea_.min_;
    const Vector2 areaMax = page.contentArea_.max_;
    const Vector2 areaSize = areaMax - areaMin;

    auto screenToXY = [&](float x, float y) -> ea::optional<Vector2>
    {
        const Ray ray = camera->GetScreenRay(x, y);
        if (Abs(ray.direction_.z_) < M_EPSILON)
            return ea::nullopt;
        const float distance = -ray.origin_.z_ / ray.direction_.z_;
        if (distance < 0.0f || !std::isfinite(distance))
            return ea::nullopt;
        const Vector3 point = ray.origin_ + ray.direction_ * distance;
        return Vector2{point.x_, point.y_};
    };

    const ea::optional<Vector2> topLeft = screenToXY(0.0f, 0.0f);
    const ea::optional<Vector2> bottomRight = screenToXY(1.0f, 1.0f);
    if (!topLeft || !bottomRight)
        return;

    const float left = std::min(topLeft->x_, bottomRight->x_);
    const float right = std::max(topLeft->x_, bottomRight->x_);
    const float bottom = std::min(topLeft->y_, bottomRight->y_);
    const float top = std::max(topLeft->y_, bottomRight->y_);
    const float pixelsPerUnit = areaSize.y_ / std::max(camera->GetOrthoSize(), 0.001f);

    // Keep the overlay bounded at extreme zoom levels while preserving the requested spacing.
    float drawSpacing = spacing;
    while (drawSpacing * pixelsPerUnit < 5.0f && drawSpacing < 100000.0f)
        drawSpacing *= 2.0f;
    while (drawSpacing * pixelsPerUnit > 120.0f && drawSpacing > spacing)
        drawSpacing *= 0.5f;

    auto toScreen = [&](const Vector3& point) -> ImVec2
    {
        const Vector2 normalized = camera->WorldToScreenPoint(point);
        return ImVec2{areaMin.x_ + normalized.x_ * areaSize.x_, areaMin.y_ + normalized.y_ * areaSize.y_};
    };

    const float firstX = std::floor(left / drawSpacing) * drawSpacing;
    const float firstY = std::floor(bottom / drawSpacing) * drawSpacing;
    ImDrawList* drawList = ui::GetWindowDrawList();
    unsigned lineCount = 0;
    for (float x = firstX; x <= right && lineCount < 2000; x += drawSpacing, ++lineCount)
    {
        const long long index = static_cast<long long>(std::llround(x / drawSpacing));
        const ImU32 color = index == 0 ? IM_COL32(95, 145, 205, 180)
            : (index % 5 == 0 ? IM_COL32(75, 95, 120, 120) : IM_COL32(55, 68, 85, 90));
        drawList->AddLine(toScreen(Vector3{x, bottom, 0.0f}), toScreen(Vector3{x, top, 0.0f}), color,
            index == 0 ? 1.5f : 1.0f);
    }

    lineCount = 0;
    for (float y = firstY; y <= top && lineCount < 2000; y += drawSpacing, ++lineCount)
    {
        const long long index = static_cast<long long>(std::llround(y / drawSpacing));
        const ImU32 color = index == 0 ? IM_COL32(95, 145, 205, 180)
            : (index % 5 == 0 ? IM_COL32(75, 95, 120, 120) : IM_COL32(55, 68, 85, 90));
        drawList->AddLine(toScreen(Vector3{left, y, 0.0f}), toScreen(Vector3{right, y, 0.0f}), color,
            index == 0 ? 1.5f : 1.0f);
    }
}

void SceneViewTab::RenderToolbar()
{
    SceneViewPage* activePage = GetActivePage();

    {
        const bool canRewind = activePage && activePage->IsSimulationActive();
        ui::BeginDisabled(!canRewind);
        if (Widgets::ToolbarButton(ICON_FA_CIRCLE_CHEVRON_LEFT, "Rewind Scene Simulation"))
            RewindSimulation();
        ui::EndDisabled();
    }

    {
        const bool isStarted = activePage && activePage->IsSimulationActive();
        const bool isUpdating = activePage && activePage->scene_->IsUpdateEnabled();
        const char* label = isUpdating ? ICON_FA_CIRCLE_PAUSE : ICON_FA_CIRCLE_PLAY;
        const char* tooltip = isUpdating ? "Pause Scene Simulation" : (isStarted ? "Resume Scene Simulation" : "Start Scene Simulation");
        ui::BeginDisabled(!activePage);
        if (Widgets::ToolbarButton(label, tooltip))
            ToggleSimulationPaused();
        ui::EndDisabled();
    }

    if (activePage)
    {
        const char* layoutLabel = multiViewportEnabled_ ? "1" : "4";
        const char* layoutTooltip = multiViewportEnabled_ ? "Use single scene viewport" : "Use four scene viewports";
        if (Widgets::ToolbarButton(layoutLabel, layoutTooltip, multiViewportEnabled_))
            multiViewportEnabled_ = !multiViewportEnabled_;

        if (Widgets::ToolbarButton("2D", "Use a dedicated orthographic XY canvas", scene2DMode_))
        {
            scene2DMode_ = !scene2DMode_;
            if (scene2DMode_)
                multiViewportEnabled_ = false;
            Configure2DView(*activePage, scene2DMode_);
        }
        ui::SameLine();
        ui::TextDisabled("%s", scene2DMode_ ? "2D XY orthographic" : "3D perspective");

        if (scene2DMode_)
        {
            ui::SameLine();
            ui::Checkbox("Grid##Scene2D", &activePage->scene2DGridEnabled_);
            ui::SameLine();
            ui::Checkbox("Snap##Scene2D", &activePage->scene2DSnapEnabled_);
            ui::SameLine();
            ui::SetNextItemWidth(78.0f);
            ui::DragFloat("Grid size##Scene2D", &activePage->scene2DGridSpacing_, 0.1f, 0.1f, 1000.0f, "%.1f");
            ui::SameLine();
            ui::SetNextItemWidth(78.0f);
            ui::DragFloat("Snap size##Scene2D", &activePage->scene2DSnapSpacing_, 0.1f, 0.1f, 1000.0f, "%.1f");
        }
    }

    Widgets::ToolbarSeparator();

    for (SceneViewAddon* addon : addonsByToolbarPriority_)
    {
        if (addon->RenderToolbar())
            Widgets::ToolbarSeparator();
    }
}

bool SceneViewTab::CanOpenResource(const ResourceFileDescriptor& desc)
{
    return desc.HasObjectType<Scene>() || desc.HasObjectType<PrefabResource>();
}

void SceneViewTab::WriteIniSettings(ImGuiTextBuffer& output)
{
    ResourceEditorTab::WriteIniSettings(output);
    WriteIntToIni(output, "MultiViewport", multiViewportEnabled_ ? 1 : 0);
    WriteIntToIni(output, "Scene2D", scene2DMode_ ? 1 : 0);
    for (SceneViewAddon* addon : addons_)
        addon->WriteIniSettings(output);
}

void SceneViewTab::ReadIniSettings(const char* line)
{
    ResourceEditorTab::ReadIniSettings(line);
    if (const auto value = ReadIntFromIni(line, "MultiViewport"))
        multiViewportEnabled_ = *value != 0;
    if (const auto value = ReadIntFromIni(line, "Scene2D"))
    {
        scene2DMode_ = *value != 0;
        if (SceneViewPage* activePage = GetActivePage())
            Configure2DView(*activePage, scene2DMode_);
    }
    for (SceneViewAddon* addon : addons_)
        addon->ReadIniSettings(line);
}

ea::optional<EditorActionFrame> SceneViewTab::PushAction(SharedPtr<EditorAction> action)
{
    SceneViewPage* activePage = GetActivePage();
    if (!activePage)
        return ea::nullopt;

    // Ignore all actions while simulating
    if (activePage->IsSimulationActive())
        return ea::nullopt;

    if (auto selectionAction = dynamic_cast<ChangeSceneSelectionAction*>(action.Get()))
        return BaseClassName::PushAction(action);

    const auto wrappedAction = MakeShared<PreserveSceneSelectionWrapper>(action, activePage);
    return BaseClassName::PushAction(wrappedAction);
}

void SceneViewTab::RenderContextMenuItems()
{
    BaseClassName::RenderContextMenuItems();

    if (SceneViewPage* activePage = GetActivePage())
    {
        contextMenuSeparator_.Reset();

        const char* rewindTitle = ICON_FA_CIRCLE_CHEVRON_LEFT " Rewind Scene Simulation";
        const char* rewindShortcut = GetHotkeyLabel(Hotkey_RewindSimulation).c_str();
        if (ui::MenuItem(rewindTitle, rewindShortcut, false, activePage->IsSimulationActive()))
            RewindSimulation();

        const char* pauseTitle = !activePage->scene_->IsUpdateEnabled()
            ? (activePage->IsSimulationActive() ? ICON_FA_CIRCLE_PLAY " Resume Scene Simulation" : ICON_FA_CIRCLE_PLAY " Start Scene Simulation")
            : ICON_FA_CIRCLE_PAUSE " Pause Scene Simulation";
        if (ui::MenuItem(pauseTitle, GetHotkeyLabel(Hotkey_TogglePaused).c_str()))
            ToggleSimulationPaused();

        if (ui::MenuItem("Compact object IDs"))
            CompactObjectIds();
    }

    contextMenuSeparator_.Add();

    for (SceneViewAddon* addon : addonsByName_)
    {
        if (addon->RenderTabContextMenu())
            contextMenuSeparator_.Reset();
    }

    contextMenuSeparator_.Add();
}

void SceneViewTab::ApplyHotkeys(HotkeyManager* hotkeyManager)
{
    BaseClassName::ApplyHotkeys(hotkeyManager);

    for (SceneViewAddon* addon : addons_)
        addon->ApplyHotkeys(hotkeyManager);
}

void SceneViewTab::OnResourceLoaded(const ea::string& resourceName)
{
    auto cache = GetSubsystem<ResourceCache>();
    auto sceneResource = cache->GetResource<SceneResourceForEditor>(resourceName);

    if (!sceneResource)
    {
        URHO3D_LOGERROR("Cannot load scene file '%s'", resourceName);
        return;
    }

    if (resourceName.ends_with(".prefab"))
        sceneResource->SetPrefab(true);

    const bool isActive = resourceName == GetActiveResourceName();
    scenes_[resourceName] = CreatePage(sceneResource, isActive);
}

void SceneViewTab::OnResourceUnloaded(const ea::string& resourceName)
{
    scenes_.erase(resourceName);
}

void SceneViewTab::OnActiveResourceChanged(const ea::string& oldResourceName, const ea::string& newResourceName)
{
    if (SceneViewPage* oldActivePage = GetPage(oldResourceName))
        oldActivePage->scene_->SetUpdateEnabled(false);

    for (const auto& [name, data] : scenes_)
        data->renderer_->SetActive(name == newResourceName);

    if (SceneViewPage* newActivePage = GetPage(newResourceName))
    {
        InspectSelection(*newActivePage);
        if (scene2DMode_)
            Configure2DView(*newActivePage, true);
    }
}

void SceneViewTab::OnResourceSaved(const ea::string& resourceName)
{
    SceneViewPage* page = GetPage(resourceName);
    if (!page)
        return;

    SavePageConfig(*page);
    SavePageScene(*page);
}

void SceneViewTab::OnResourceShallowSaved(const ea::string& resourceName)
{
    SceneViewPage* page = GetPage(resourceName);
    if (!page)
        return;

    SavePageConfig(*page);

    if (GetActiveResourceName() == resourceName)
        SavePagePreview(*page);
}

void SceneViewTab::SavePageScene(SceneViewPage& page) const
{
    const bool isLegacyScene = page.resource_->GetName().ends_with(".xml");

    page.scene_->SetUpdateEnabled(false);
    RecalculateSceneNextIds(page.scene_);

    VectorBuffer buffer;
    if (isLegacyScene)
    {
        XMLFile xmlFile(context_);
        XMLElement rootElement = xmlFile.GetOrCreateRoot("scene");
        page.scene_->SaveXML(rootElement);
        xmlFile.Save(buffer);
    }
    else
    {
        page.resource_->Save(buffer);
    }

    auto sharedBuffer = ea::make_shared<ByteVector>(ea::move(buffer.GetBuffer()));

    const WeakPtr<SceneViewPage> weakPage{&page};

    auto project = GetProject();
    project->SaveFileDelayed(page.resource_->GetAbsoluteFileName(), page.resource_->GetName(), sharedBuffer,
        [this, weakPage](const ea::string& _, const ea::string& resourceName, bool& needReload)
    {
        if (SceneViewPage* page = weakPage)
        {
            // Force reload of the scene and/or prefabs, but ignore it in SceneViewTab
            page->ignoreNextReload_ = true;
            needReload = true;
        }
    });
}

void SceneViewTab::SavePagePreview(SceneViewPage& page) const
{
    Texture2D* texture = page.renderer_->GetTexture();
    SharedPtr<Image> image = texture->GetImage();

    if (!image)
        return;

    // Crop to square
    const IntVector2 size{image->GetWidth(), image->GetHeight()};
    const int minSide = ea::min(size.x_, size.y_);
    const IntVector2 excess = size - IntVector2::ONE * minSide;
    const IntVector2 offsetTopLeft = excess / 2;
    const IntVector2 offsetRightBottom = excess - offsetTopLeft;

    const IntRect rect{offsetTopLeft, size - offsetRightBottom};
    SharedPtr<Image> croppedImage = image->GetSubimage(rect);

    const ea::string& path = GetProject()->GetPreviewPngPath();
    croppedImage->SavePNG(path);
}

void SceneViewTab::PreRenderUpdate()
{
    if (SceneViewPage* activePage = GetActivePage())
    {
        if (!componentSelection_)
            activePage->selection_.ConvertToNodes();
        activePage->BeginSelection();
    }
}

void SceneViewTab::PostRenderUpdate()
{
    if (SceneViewPage* activePage = GetActivePage())
        activePage->EndSelection(this);
}

void SceneViewTab::RenderContent()
{
    SceneViewPage* activePage = GetActivePage();
    if (!activePage)
        return;

    if (!activePage->scene_->HasComponent<DebugRenderer>())
    {
        auto debug = activePage->scene_->GetOrCreateComponent<DebugRenderer>();
        debug->SetTemporary(true);
        debug->SetLineAntiAlias(true);
    }

    if (multiViewportEnabled_)
    {
        activePage->renderer_->SetActive(false);
        RenderMultiViewport(*activePage);
        return;
    }

    for (SceneViewPane& pane : activePage->multiViewports_)
        pane.renderer_->SetActive(false);

    const IntVector2 contentSize = GetContentSize();
    if (contentSize.x_ == 0 || contentSize.y_ == 0)
        return;

    activePage->renderer_->SetActive(true);
    activePage->renderer_->SetTextureSize(contentSize);
    activePage->renderer_->Update();

    const ImVec2 basePosition = ui::GetCursorPos();

    Texture2D* sceneTexture = activePage->renderer_->GetTexture();
    ui::SetCursorPos(basePosition);
    Widgets::ImageItem(sceneTexture, ToImGui(sceneTexture->GetSize()));

    const auto contentAreaMin = ToVector2(ui::GetItemRectMin());
    const auto contentAreaMax = ToVector2(ui::GetItemRectMax());
    activePage->contentArea_ = Rect{contentAreaMin, contentAreaMax};
    RenderScene2DGrid(*activePage);

    UpdateCameraRay();
    UpdateAddons(*activePage);
}

void SceneViewTab::RenderMultiViewport(SceneViewPage& page)
{
    const ImVec2 available = ui::GetContentRegionAvail();
    const float gap = ui::GetStyle().ItemSpacing.x;
    const ImVec2 paneSize{
        std::max(1.0f, (available.x - gap) * 0.5f),
        std::max(1.0f, (available.y - gap) * 0.5f)};

    for (unsigned index = 0; index < page.multiViewports_.size(); ++index)
    {
        if ((index & 1u) != 0)
            ui::SameLine();
        ui::PushID(static_cast<int>(index));
        auto& pane = page.multiViewports_[index];
        if (ui::BeginChild("##SceneViewportPane", paneSize, true))
        {
            ui::TextUnformatted(pane.label_.c_str());
            const ImVec2 imageSize = ui::GetContentRegionAvail();
            const IntVector2 textureSize{
                std::max(1, static_cast<int>(imageSize.x)),
                std::max(1, static_cast<int>(imageSize.y))};
            pane.renderer_->SetActive(true);
            pane.renderer_->SetTextureSize(textureSize);
            pane.renderer_->Update();
            Texture2D* texture = pane.renderer_->GetTexture();
            Widgets::ImageItem(texture, ToImGui(texture->GetSize()));
            pane.contentArea_ = Rect{ToVector2(ui::GetItemRectMin()), ToVector2(ui::GetItemRectMax())};

            const bool hovered = ui::IsItemHovered();
            if (hovered && ui::IsMouseClicked(MOUSEB_LEFT))
                activeViewportIndex_ = index;
            if (hovered && activeViewportIndex_ == index)
            {
                const ImGuiIO& io = ui::GetIO();
                Node* cameraNode = pane.renderer_->GetCameraNode();
                if (ui::IsMouseDown(MOUSEB_MIDDLE))
                {
                    const float scale = pane.renderer_->GetCamera()->IsOrthographic()
                        ? pane.orthoSize_ / std::max(1.0f, imageSize.y) : 0.04f;
                    const Vector3 dragDelta{io.MouseDelta.x * scale, -io.MouseDelta.y * scale, 0.0f};
                    if (pane.type_ == SceneViewPaneType::Top)
                        cameraNode->Translate(Vector3{-dragDelta.x_, 0.0f, dragDelta.y_}, TS_WORLD);
                    else if (pane.type_ == SceneViewPaneType::Right)
                        cameraNode->Translate(Vector3{0.0f, dragDelta.y_, dragDelta.x_}, TS_WORLD);
                    else
                        cameraNode->Translate(Vector3{-dragDelta.x_, dragDelta.y_, 0.0f}, TS_WORLD);
                }
                if (pane.renderer_->GetCamera()->IsOrthographic() && io.MouseWheel != 0.0f)
                {
                    pane.orthoSize_ = std::max(1.0f, pane.orthoSize_ * (1.0f - io.MouseWheel * 0.1f));
                    pane.renderer_->GetCamera()->SetOrthoSize(pane.orthoSize_);
                }
                page.contentArea_ = pane.contentArea_;
                UpdateCameraRay(pane.renderer_);
                UpdateAddons(page);
            }
        }
        ui::EndChild();
        ui::PopID();
    }
}

void SceneViewTab::UpdateAddons(SceneViewPage& page)
{
    bool mouseConsumed = false;

    if (UpdateDropToScene())
        mouseConsumed = true;

    for (SceneViewAddon* addon : addonsByInputPriority_)
        addon->ProcessInput(page, mouseConsumed);

    for (SceneViewAddon* addon : addons_)
        addon->Render(page);
}

void SceneViewTab::UpdateCameraRay(SceneRendererToTexture* renderer)
{
    SceneViewPage* activePage = GetActivePage();
    if (!activePage)
        return;

    ImGuiIO& io = ui::GetIO();
    Camera* camera = renderer ? renderer->GetCamera() : activePage->renderer_->GetCamera();

    const ImRect viewportRect{ui::GetItemRectMin(), ui::GetItemRectMax()};
    const auto pos = ToVector2((io.MousePos - viewportRect.Min) / viewportRect.GetSize());
    activePage->cameraRay_ = camera->GetScreenRay(pos.x_, pos.y_);
}

bool SceneViewTab::UpdateDropToScene()
{
    SceneViewPage* activePage = GetActivePage();
    if (activePage && ui::BeginDragDropTarget())
    {
        const auto payload = DragDropPayload::Get();

        if (!dragAndDropAddon_)
        {
            const auto isPayloadSupported = [&](SceneViewAddon* addon) { return addon->IsDragDropPayloadSupported(*activePage, payload); };
            const auto iter = ea::find_if(addonsByInputPriority_.begin(), addonsByInputPriority_.end(), isPayloadSupported);

            dragAndDropAddon_ = iter != addonsByInputPriority_.end() ? *iter : nullptr;
            if (dragAndDropAddon_)
                dragAndDropAddon_->BeginDragDrop(*activePage, payload);
        }

        if (dragAndDropAddon_)
        {
            if (ui::AcceptDragDropPayload(DragDropPayloadType.c_str(), ImGuiDragDropFlags_AcceptBeforeDelivery))
            {
                dragAndDropAddon_->UpdateDragDrop(payload);
                if (ui::GetDragDropPayload()->IsDelivery())
                {
                    dragAndDropAddon_->CompleteDragDrop(payload);
                    dragAndDropAddon_ = nullptr;
                }
            }
        }

        ui::EndDragDropTarget();
    }
    else if (dragAndDropAddon_)
    {
        dragAndDropAddon_->CancelDragDrop();
        dragAndDropAddon_ = nullptr;
    }
    return dragAndDropAddon_ != nullptr;
}

void SceneViewTab::InspectSelection(SceneViewPage& page)
{
    auto project = GetProject();
    auto request = MakeShared<InspectNodeComponentRequest>(context_, page.selection_.GetNodesAndScenes(), page.selection_.GetComponents());
    project->ProcessRequest(request, this);
}

SceneViewPage* SceneViewTab::GetPage(const ea::string& resourceName)
{
    const auto iter = scenes_.find(resourceName);
    return iter != scenes_.end() ? iter->second : nullptr;
}

SceneViewPage* SceneViewTab::GetPage(Scene* scene)
{
    const auto iter = ea::find_if(scenes_.begin(), scenes_.end(), [scene](const auto& pair) { return pair.second->scene_ == scene; });
    return iter != scenes_.end() ? iter->second : nullptr;
}

SceneViewPage* SceneViewTab::GetActivePage()
{
    return GetPage(GetActiveResourceName());
}

SharedPtr<SceneViewPage> SceneViewTab::CreatePage(SceneResource* sceneResource, bool isActive)
{
    auto page = MakeShared<SceneViewPage>(sceneResource);

    sceneResource->OnReloadBegin.Subscribe(page.Get(), [this](SceneViewPage* page, bool& cancelReload)
    {
        if (page->ignoreNextReload_ || IsResourceUnsaved(page->resource_->GetName()))
        {
            page->ignoreNextReload_ = false;
            cancelReload = true;
            return;
        }

        page->loadingSelection_ = page->selection_.Pack();
    });

    sceneResource->OnReloadEnd.Subscribe(page.Get(), [this](SceneViewPage* page, bool success)
    {
        if (success && page->loadingSelection_)
        {
            page->selection_.Load(page->scene_, *page->loadingSelection_);
            page->loadingSelection_ = ea::nullopt;
        }
    });

    page->renderer_->SetActive(isActive);

    WeakPtr<SceneViewPage> weakPage{page};
    page->selection_.OnChanged.Subscribe(this, [weakPage](SceneViewTab* self)
    {
        if (weakPage)
            self->InspectSelection(*weakPage);
    });

    LoadPageConfig(*page);
    for (SceneViewAddon* addon : addons_)
        addon->Initialize(*page);
    return page;
}

void SceneViewTab::SavePageConfig(const SceneViewPage& page) const
{
    auto jsonFile = MakeShared<JSONFile>(context_);
    jsonFile->SaveObject("Scene", page, this);
    jsonFile->SaveFile(page.cfgFileName_);
}

void SceneViewTab::LoadPageConfig(SceneViewPage& page) const
{
    auto fs = GetSubsystem<FileSystem>();
    auto jsonFile = MakeShared<JSONFile>(context_);
    if (fs->FileExists(page.cfgFileName_))
    {
        if (jsonFile->LoadFile(page.cfgFileName_))
            jsonFile->LoadObject("Scene", page, this);
    }
}

void SceneViewTab::BeginPluginReload()
{
    for (const auto& [_, page] : scenes_)
    {
        page->archivedScene_ = PackedSceneData::FromScene(page->scene_);
        page->archivedSelection_ = page->selection_.Pack();
        page->scene_->Clear();
    }
}

void SceneViewTab::EndPluginReload()
{
    for (const auto& [_, page] : scenes_)
    {
        page->scene_->Clear();
        page->archivedScene_.ToScene(page->scene_);
        page->selection_.Load(page->scene_, page->archivedSelection_);
    }
}

SimulateSceneAction::SimulateSceneAction(SceneViewPage* page)
    : page_(page)
{
    oldData_ = PackedSceneData::FromScene(page->scene_);
    page->selection_.Save(oldSelection_);
}

void SimulateSceneAction::Complete(bool force)
{
    if (!force)
        return;

    isComplete_ = true;
    if (page_)
    {
        page_->scene_->SetElapsedTime(0.0f);
        page_->scene_->SetUpdateEnabled(false);
        newData_ = PackedSceneData::FromScene(page_->scene_);
        page_->selection_.Save(newSelection_);
    }
}

bool SimulateSceneAction::CanUndoRedo() const
{
    return page_ != nullptr && newData_.HasSceneData();
}

void SimulateSceneAction::SetState(const PackedSceneData& data, const PackedSceneSelection& selection) const
{
    page_->scene_->SetUpdateEnabled(false);
    data.ToScene(page_->scene_);
    page_->selection_.Load(page_->scene_, selection);
}

void SimulateSceneAction::Redo() const
{
    SetState(newData_, newSelection_);
}

void SimulateSceneAction::Undo() const
{
    SetState(oldData_, oldSelection_);
}

ChangeSceneSelectionAction::ChangeSceneSelectionAction(SceneViewPage* page,
    const PackedSceneSelection& oldSelection, const PackedSceneSelection& newSelection)
    : page_(page)
    , oldSelection_(oldSelection)
    , newSelection_(newSelection)
{
}

void ChangeSceneSelectionAction::Redo() const
{
    SetSelection(newSelection_);
}

void ChangeSceneSelectionAction::Undo() const
{
    SetSelection(oldSelection_);
}

void ChangeSceneSelectionAction::SetSelection(const PackedSceneSelection& selection) const
{
    if (page_)
    {
        page_->selection_.Load(page_->scene_, selection);
        page_->BeginSelection();
    }
}

bool ChangeSceneSelectionAction::MergeWith(const EditorAction& other)
{
    auto otherAction = dynamic_cast<const ChangeSceneSelectionAction*>(&other);
    if (!otherAction)
        return false;

    if (page_ != otherAction->page_)
        return false;

    newSelection_ = otherAction->newSelection_;
    return true;
}

PreserveSceneSelectionWrapper::PreserveSceneSelectionWrapper(SharedPtr<EditorAction> action, SceneViewPage* page)
    : BaseEditorActionWrapper(action)
    , page_(page)
    , selection_(page->selection_.Pack())
{
}

bool PreserveSceneSelectionWrapper::CanRedo() const
{
    return page_ && action_->CanRedo();
}

void PreserveSceneSelectionWrapper::Redo() const
{
    action_->Redo();
    page_->selection_.Load(page_->scene_, selection_);
}

bool PreserveSceneSelectionWrapper::CanUndo() const
{
    return page_ && action_->CanUndo();
}

void PreserveSceneSelectionWrapper::Undo() const
{
    action_->Undo();
    page_->selection_.Load(page_->scene_, selection_);
}

bool PreserveSceneSelectionWrapper::MergeWith(const EditorAction& other)
{
    const auto otherWrapper = dynamic_cast<const PreserveSceneSelectionWrapper*>(&other);
    if (!otherWrapper)
        return false;

    if (page_ != otherWrapper->page_ || selection_ != otherWrapper->selection_)
        return false;

    return action_->MergeWith(*otherWrapper->action_);
}

ea::vector<RayQueryResult> QueryGeometriesFromScene(Scene* scene, const Ray& ray, RayQueryLevel level, float maxDistance, unsigned viewMask)
{
    ea::vector<RayQueryResult> results;
    RayOctreeQuery query(results, ray, level, maxDistance, DRAWABLE_GEOMETRY, viewMask);
    if (auto octree = scene->GetComponent<Octree>())
        octree->Raycast(query);
    return results;
}

}
