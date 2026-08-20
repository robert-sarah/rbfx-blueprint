// SPDX-License-Identifier: MIT

#include "VFXGraphTab.h"

#include "../Core/EditorIcons.h"
#include "../Core/EditorTheme.h"
#include "../Project/Project.h"

#include <Urho3D/Core/Timer.h>
#include <Urho3D/Resource/ResourceCache.h>
#include <Urho3D/SystemUI/SystemUI.h>

namespace Urho3D
{

namespace
{

const char* GetNodeTypeName(VFXNodeType type)
{
    switch (type)
    {
    case VFXNodeType::SpawnRate: return "Spawn Rate";
    case VFXNodeType::InitialVelocity: return "Initial Velocity";
    case VFXNodeType::Force: return "Force";
    case VFXNodeType::Drag: return "Drag";
    case VFXNodeType::ColorOverLife: return "Color Over Life";
    case VFXNodeType::SizeOverLife: return "Size Over Life";
    case VFXNodeType::RibbonTrail: return "Ribbon Trail";
    case VFXNodeType::Output: return "Output";
    default: return "Unknown";
    }
}

class VFXGraphSnapshotAction final : public EditorAction
{
public:
    VFXGraphSnapshotAction(VFXGraphTab* tab, const JSONValue& before, const JSONValue& after)
        : tab_(tab)
        , before_(before)
        , after_(after)
    {
    }

    void Redo() const override
    {
        if (tab_)
            tab_->ApplyGraphSnapshot(after_);
    }

    void Undo() const override
    {
        if (tab_)
            tab_->ApplyGraphSnapshot(before_);
    }

private:
    WeakPtr<VFXGraphTab> tab_;
    JSONValue before_;
    JSONValue after_;
};

} // namespace

void Foundation_VFXGraphTab(Context* context, Project* project)
{
    project->AddTab(MakeShared<VFXGraphTab>(context));
}

VFXGraphTab::VFXGraphTab(Context* context)
    : ResourceEditorTab(context, "VFX Graph", "d6bb45b1-1a0e-4ea7-8a5c-6b6df76f8d4d",
          EditorTabFlag::OpenByDefault, EditorTabPlacement::DockCenter)
{
    ResetDemoGraph();
}

bool VFXGraphTab::CanOpenResource(const ResourceFileDescriptor& desc)
{
    return desc.HasObjectType<VFXGraphResource>();
}

VFXGraph& VFXGraphTab::GetGraph()
{
    return resource_ ? resource_->GetGraph() : previewGraph_;
}

const VFXGraph& VFXGraphTab::GetGraph() const
{
    return resource_ ? resource_->GetGraph() : previewGraph_;
}

JSONValue VFXGraphTab::CaptureGraph() const
{
    if (resource_)
        return resource_->ToJSON();

    VFXGraphResource temporary(GetContext());
    temporary.SetGraph(previewGraph_);
    return temporary.ToJSON();
}

void VFXGraphTab::ApplyGraphSnapshot(const JSONValue& snapshot)
{
    ea::string error;
    if (resource_)
    {
        if (!resource_->FromJSON(snapshot, &error))
        {
            status_ = Format("Unable to restore VFX graph: {}", error);
            return;
        }
    }
    else
    {
        VFXGraphResource temporary(GetContext());
        if (!temporary.FromJSON(snapshot, &error))
        {
            status_ = Format("Unable to restore VFX graph: {}", error);
            return;
        }
        previewGraph_ = temporary.GetGraph();
    }

    if (!GetGraph().GetNode(selectedNodeId_))
        selectedNodeId_ = GetGraph().GetOutputNodeId();
    status_ = "VFX graph state restored";
}

void VFXGraphTab::CommitGraphEdit(const JSONValue& before, const ea::string& status)
{
    const JSONValue after = CaptureGraph();
    if (before == after)
        return;

    PushAction<VFXGraphSnapshotAction>(this, before, after);
    status_ = status;
    validationError_.clear();
}

void VFXGraphTab::ResetDemoGraph()
{
    VFXGraph graph;
    graph.SetSimulationMode(VFXSimulationMode::CPU);
    graph.SetMaxParticles(2048);
    graph.SetSpawnRate(80.0f);
    graph.SetParticleLifetime(2.5f);
    graph.SetInitialVelocity(Vector3(0.0f, 3.0f, 0.0f));
    graph.SetForce(Vector3(0.0f, -2.0f, 0.0f));
    graph.SetDrag(0.1f);
    graph.SetRibbonTrailLength(24);

    graph.AddNode("Spawn Rate", VFXNodeType::SpawnRate);
    graph.AddNode("Initial Velocity", VFXNodeType::InitialVelocity);
    graph.AddNode("Gravity Force", VFXNodeType::Force);
    graph.AddNode("Color Over Life", VFXNodeType::ColorOverLife);
    graph.AddNode("Size Over Life", VFXNodeType::SizeOverLife);
    graph.AddNode("Ribbon Trail", VFXNodeType::RibbonTrail);
    const unsigned output = graph.AddNode("Particle Output", VFXNodeType::Output);
    graph.SetOutputNode(output);

    if (resource_)
    {
        const JSONValue before = CaptureGraph();
        resource_->SetGraph(graph);
        selectedNodeId_ = output;
        CommitGraphEdit(before, "Reset VFX graph to template");
    }
    else
    {
        previewGraph_ = graph;
        selectedNodeId_ = output;
        status_ = "VFX graph template ready";
    }
    ValidateGraph();
}

void VFXGraphTab::ValidateGraph()
{
    validationError_.clear();
    if (GetGraph().Compile(&validationError_))
        status_ = "VFX graph is valid";
    else
        status_ = "VFX graph validation failed";
}

void VFXGraphTab::TogglePreview()
{
    VFXGraph& graph = GetGraph();
    if (graph.IsPlaying())
    {
        graph.Stop();
        status_ = "VFX preview stopped";
    }
    else if (graph.Play())
    {
        status_ = "VFX preview playing";
    }
    else
    {
        status_ = "VFX preview could not start";
    }
}

void VFXGraphTab::RenderToolbar()
{
    EditorTheme::PushToolbarColors();
    if (ui::Button(EditorIcons::ValidateLabel))
        ValidateGraph();
    ui::SameLine();
    if (ui::Button(GetGraph().IsPlaying() ? ICON_FA_STOP " Stop Preview" : ICON_FA_PLAY " Play Preview"))
        TogglePreview();
    ui::SameLine();
    if (ui::Button(ICON_FA_BROOM " Clear Preview"))
    {
        GetGraph().ClearParticles();
        status_ = "VFX preview particles cleared";
    }
    ui::SameLine();
    if (ui::Button(EditorIcons::ResetLabel))
        ResetDemoGraph();
    EditorTheme::PopToolbarColors();
    ui::SameLine();
    ui::TextColored(EditorThemeColors::ToColor(EditorThemeColors::TextMuted), "%s", status_.c_str());
}

void VFXGraphTab::RenderNodeList(VFXGraph& graph)
{
    ui::Text("Nodes");
    if (ui::BeginTable("VFXGraphNodes", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV
            | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY, ImVec2(0, 250)))
    {
        ui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 42.0f);
        ui::TableSetupColumn("Name");
        ui::TableSetupColumn("Type");
        ui::TableHeadersRow();
        for (const VFXNode& node : graph.GetNodes())
        {
            if (!node.id)
                continue;

            ui::TableNextRow();
            ui::TableSetColumnIndex(0);
            ui::PushID(static_cast<int>(node.id));
            ui::Text("%u", node.id);
            ui::TableSetColumnIndex(1);
            if (ui::Selectable(node.name.c_str(), selectedNodeId_ == node.id, ImGuiSelectableFlags_SpanAllColumns))
                selectedNodeId_ = node.id;
            ui::TableSetColumnIndex(2);
            ui::TextUnformatted(GetNodeTypeName(node.type));
            ui::PopID();
        }
        ui::EndTable();
    }

    static const char* nodeTypes[] = {"Spawn Rate", "Initial Velocity", "Force", "Drag", "Color Over Life",
        "Size Over Life", "Ribbon Trail", "Output"};
    int nodeType = static_cast<int>(newNodeType_);
    ui::SetNextItemWidth(-1.0f);
    if (ui::Combo("##VFXGraphNodeType", &nodeType, nodeTypes, IM_ARRAYSIZE(nodeTypes)))
        newNodeType_ = static_cast<unsigned>(nodeType);
    EditorTheme::PushToolbarColors(true);
    if (ui::Button(EditorIcons::AddLabel, ImVec2(-1.0f, 0.0f)))
    {
        const JSONValue before = CaptureGraph();
        const VFXNodeType type = static_cast<VFXNodeType>(newNodeType_);
        const unsigned id = graph.AddNode("New VFX Node", type);
        if (type == VFXNodeType::Output)
            graph.SetOutputNode(id);
        selectedNodeId_ = id;
        CommitGraphEdit(before, "Added VFX graph node");
    }
    EditorTheme::PopToolbarColors();
}

void VFXGraphTab::RenderNodeInspector(VFXGraph& graph)
{
    ui::Text("Node Inspector");
    VFXNode* node = graph.GetNode(selectedNodeId_);
    if (!node)
    {
        ui::TextDisabled("Select a node to edit its metadata.");
        return;
    }

    const JSONValue before = CaptureGraph();
    ui::Text("ID: %u", node->id);
    ui::Text("Type: %s", GetNodeTypeName(node->type));
    ui::InputText("Name", &node->name);
    ui::DragFloat("Scalar", &node->scalarValue, 0.01f, -10000.0f, 10000.0f, "%.3f");
    ui::DragFloat("Vector X", &node->vectorValue.x_, 0.05f, -10000.0f, 10000.0f, "%.3f");
    ui::DragFloat("Vector Y", &node->vectorValue.y_, 0.05f, -10000.0f, 10000.0f, "%.3f");
    ui::DragFloat("Vector Z", &node->vectorValue.z_, 0.05f, -10000.0f, 10000.0f, "%.3f");
    if (before != CaptureGraph())
        CommitGraphEdit(before, "Edited VFX graph node");

    EditorTheme::PushToolbarColors();
    if (node->type != VFXNodeType::Output && ui::Button(ICON_FA_BULLSEYE " Set Selected as Output"))
    {
        const JSONValue outputBefore = CaptureGraph();
        if (graph.SetOutputNode(node->id))
            CommitGraphEdit(outputBefore, "Changed VFX graph output");
    }

    if (ui::Button(EditorIcons::RemoveLabel))
    {
        const JSONValue removeBefore = CaptureGraph();
        const unsigned removedId = node->id;
        if (graph.RemoveNode(removedId))
        {
            selectedNodeId_ = graph.GetOutputNodeId();
            CommitGraphEdit(removeBefore, "Removed VFX graph node");
        }
    }
    EditorTheme::PopToolbarColors();
}

void VFXGraphTab::RenderSimulationControls(VFXGraph& graph)
{
    ui::Separator();
    ui::Text("Simulation");
    const JSONValue before = CaptureGraph();

    static const char* modes[] = {"CPU", "GPU"};
    int mode = static_cast<int>(graph.GetSimulationMode());
    if (ui::Combo("Mode", &mode, modes, IM_ARRAYSIZE(modes)))
        graph.SetSimulationMode(static_cast<VFXSimulationMode>(mode));

    int maxParticles = static_cast<int>(graph.GetMaxParticles());
    if (ui::SliderInt("Max Particles", &maxParticles, 1, 100000))
        graph.SetMaxParticles(static_cast<unsigned>(ea::max(maxParticles, 1)));

    float spawnRate = graph.GetSpawnRate();
    if (ui::DragFloat("Spawn Rate", &spawnRate, 1.0f, 0.0f, 100000.0f, "%.1f"))
        graph.SetSpawnRate(spawnRate);

    float lifetime = graph.GetParticleLifetime();
    if (ui::DragFloat("Lifetime", &lifetime, 0.01f, 0.01f, 3600.0f, "%.2f"))
        graph.SetParticleLifetime(lifetime);

    Vector3 initialVelocity = graph.GetInitialVelocity();
    if (ui::DragFloat("Initial Velocity X", &initialVelocity.x_, 0.05f, -10000.0f, 10000.0f, "%.2f")
        || ui::DragFloat("Initial Velocity Y", &initialVelocity.y_, 0.05f, -10000.0f, 10000.0f, "%.2f")
        || ui::DragFloat("Initial Velocity Z", &initialVelocity.z_, 0.05f, -10000.0f, 10000.0f, "%.2f"))
    {
        graph.SetInitialVelocity(initialVelocity);
    }

    Vector3 force = graph.GetForce();
    if (ui::DragFloat("Force X", &force.x_, 0.05f, -10000.0f, 10000.0f, "%.2f")
        || ui::DragFloat("Force Y", &force.y_, 0.05f, -10000.0f, 10000.0f, "%.2f")
        || ui::DragFloat("Force Z", &force.z_, 0.05f, -10000.0f, 10000.0f, "%.2f"))
    {
        graph.SetForce(force);
    }

    float drag = graph.GetDrag();
    if (ui::DragFloat("Drag", &drag, 0.01f, 0.0f, 1.0f, "%.3f"))
        graph.SetDrag(drag);

    int ribbonLength = static_cast<int>(graph.GetRibbonTrailLength());
    if (ui::SliderInt("Ribbon Length", &ribbonLength, 0, 256))
        graph.SetRibbonTrailLength(static_cast<unsigned>(ea::max(ribbonLength, 0)));

    if (before != CaptureGraph())
        CommitGraphEdit(before, "Edited VFX simulation settings");
}

void VFXGraphTab::RenderPreviewStats(const VFXGraph& graph)
{
    ui::Separator();
    ui::Text("Preview");
    ui::Text("State: %s", graph.IsPlaying() ? "Playing" : "Stopped");
    ui::Text("Particles: %u / %u", static_cast<unsigned>(graph.GetParticles().size()), graph.GetMaxParticles());
    ui::Text("Ribbon points: %u", static_cast<unsigned>(graph.GetRibbonPoints().size()));
}

void VFXGraphTab::RenderContent()
{
    VFXGraph& graph = GetGraph();
    if (graph.IsPlaying())
    {
        const float timeStep = GetSubsystem<Time>() ? GetSubsystem<Time>()->GetTimeStep() : 1.0f / 60.0f;
        graph.Update(timeStep > 0.0f ? timeStep : 1.0f / 60.0f);
    }

    if (ui::BeginTable("VFXGraphEditorLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
    {
        ui::TableSetupColumn("Graph", ImGuiTableColumnFlags_WidthFixed, 340.0f);
        ui::TableSetupColumn("Inspector");
        ui::TableNextRow();
        ui::TableSetColumnIndex(0);
        RenderNodeList(graph);
        RenderSimulationControls(graph);
        ui::TableSetColumnIndex(1);
        RenderNodeInspector(graph);
        RenderPreviewStats(graph);
        ui::EndTable();
    }

    if (!validationError_.empty())
    {
        EditorTheme::PushDiagnosticText(true);
        ui::Text("Error: %s", validationError_.c_str());
        EditorTheme::PopDiagnosticText();
    }
}

void VFXGraphTab::RenderContextMenuItems()
{
    if (ui::MenuItem("Validate VFX graph"))
        ValidateGraph();
    if (ui::MenuItem("Reset VFX graph template"))
        ResetDemoGraph();
    if (ui::MenuItem(GetGraph().IsPlaying() ? "Stop VFX preview" : "Play VFX preview"))
        TogglePreview();
}

void VFXGraphTab::OnResourceLoaded(const ea::string& resourceName)
{
    resource_ = GetSubsystem<ResourceCache>()->GetResource<VFXGraphResource>(resourceName);
    selectedNodeId_ = resource_ ? resource_->GetGraph().GetOutputNodeId() : 0;
    validationError_.clear();
    status_ = resource_ ? "VFX graph loaded" : "Unable to load VFX graph";
}

void VFXGraphTab::OnResourceUnloaded(const ea::string& resourceName)
{
    if (resourceName == GetActiveResourceName())
        resource_.Reset();
}

void VFXGraphTab::OnActiveResourceChanged(const ea::string& oldResourceName, const ea::string& newResourceName)
{
    if (newResourceName.empty())
    {
        resource_.Reset();
        return;
    }
    resource_ = GetSubsystem<ResourceCache>()->GetResource<VFXGraphResource>(newResourceName);
    selectedNodeId_ = resource_ ? resource_->GetGraph().GetOutputNodeId() : 0;
}

void VFXGraphTab::OnResourceSaved(const ea::string& resourceName)
{
    status_ = Format("Saved {}", resourceName);
}

void VFXGraphTab::OnResourceShallowSaved(const ea::string& resourceName)
{
    (void)resourceName;
}

} // namespace Urho3D
