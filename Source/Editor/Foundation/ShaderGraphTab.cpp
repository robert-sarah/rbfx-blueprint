// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#include "ShaderGraphTab.h"

#include "../Core/EditorIcons.h"
#include "../Core/EditorTheme.h"
#include "../Project/Project.h"

#include <Urho3D/Resource/ResourceCache.h>
#include <Urho3D/SystemUI/SystemUI.h>

namespace Urho3D
{

namespace
{

const char* GetNodeKindName(ShaderGraphNodeKind kind)
{
    switch (kind)
    {
    case ShaderGraphNodeKind::Constant: return "Constant";
    case ShaderGraphNodeKind::Parameter: return "Parameter";
    case ShaderGraphNodeKind::Add: return "Add";
    case ShaderGraphNodeKind::Multiply: return "Multiply";
    case ShaderGraphNodeKind::Lerp: return "Lerp";
    case ShaderGraphNodeKind::TextureSample: return "Texture Sample";
    case ShaderGraphNodeKind::Output: return "Output";
    default: return "Unknown";
    }
}

const char* GetValueTypeName(ShaderGraphValueType type)
{
    switch (type)
    {
    case ShaderGraphValueType::Float: return "Float";
    case ShaderGraphValueType::Vector2: return "Vector2";
    case ShaderGraphValueType::Vector3: return "Vector3";
    case ShaderGraphValueType::Vector4: return "Vector4";
    case ShaderGraphValueType::Color: return "Color";
    case ShaderGraphValueType::Bool: return "Bool";
    case ShaderGraphValueType::Texture2D: return "Texture2D";
    default: return "Unknown";
    }
}

class ShaderGraphSnapshotAction final : public EditorAction
{
public:
    ShaderGraphSnapshotAction(ShaderGraphTab* tab, const JSONValue& before, const JSONValue& after)
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
    WeakPtr<ShaderGraphTab> tab_;
    JSONValue before_;
    JSONValue after_;
};

} // namespace

void Foundation_ShaderGraphTab(Context* context, Project* project)
{
    project->AddTab(MakeShared<ShaderGraphTab>(context));
}

ShaderGraphTab::ShaderGraphTab(Context* context)
    : ResourceEditorTab(context, "Shader Graph", "f0e5d0c1-31f5-4d70-bb75-5bfc9aa4f801",
          EditorTabFlag::OpenByDefault, EditorTabPlacement::DockCenter)
{
    ResetDemoGraph();
}

bool ShaderGraphTab::CanOpenResource(const ResourceFileDescriptor& desc)
{
    return desc.HasObjectType<ShaderGraphResource>();
}

ShaderGraph& ShaderGraphTab::GetGraph()
{
    return resource_ ? resource_->GetGraph() : previewGraph_;
}

const ShaderGraph& ShaderGraphTab::GetGraph() const
{
    return resource_ ? resource_->GetGraph() : previewGraph_;
}

JSONValue ShaderGraphTab::CaptureGraph() const
{
    if (resource_)
        return resource_->ToJSON();

    ShaderGraphResource temporary(GetContext());
    temporary.SetGraph(previewGraph_);
    return temporary.ToJSON();
}

void ShaderGraphTab::ApplyGraphSnapshot(const JSONValue& snapshot)
{
    ea::string error;
    if (resource_)
    {
        if (!resource_->FromJSON(snapshot, &error))
        {
            status_ = Format("Unable to restore shader graph: {}", error);
            return;
        }
    }
    else
    {
        ShaderGraphResource temporary(GetContext());
        if (!temporary.FromJSON(snapshot, &error))
        {
            status_ = Format("Unable to restore shader graph: {}", error);
            return;
        }
        previewGraph_ = temporary.GetGraph();
    }

    if (!GetGraph().GetNode(selectedNodeId_))
        selectedNodeId_ = GetGraph().GetOutputNode() ? GetGraph().GetOutputNode()->id : 0;
    status_ = "Shader graph state restored";
}

void ShaderGraphTab::CommitGraphEdit(const JSONValue& before, const ea::string& status)
{
    const JSONValue after = CaptureGraph();
    if (before == after)
        return;

    PushAction<ShaderGraphSnapshotAction>(this, before, after);
    status_ = status;
    validationError_.clear();
}

void ShaderGraphTab::ResetDemoGraph()
{
    ShaderGraph graph;
    graph.SetParameter({"Tint", ShaderGraphValueType::Color, Variant(Color::WHITE)});
    const unsigned parameter = graph.AddNode("Tint", ShaderGraphNodeKind::Parameter, ShaderGraphValueType::Color,
        Variant(ea::string("Tint")));
    const unsigned output = graph.AddNode("Surface Output", ShaderGraphNodeKind::Output, ShaderGraphValueType::Color);
    graph.Connect(parameter, "value", output, "color");
    graph.SetOutputNode(output);

    if (resource_)
    {
        const JSONValue before = CaptureGraph();
        resource_->SetGraph(graph);
        selectedNodeId_ = parameter;
        CommitGraphEdit(before, "Reset shader graph to template");
    }
    else
    {
        previewGraph_ = graph;
        selectedNodeId_ = parameter;
        status_ = "Shader graph template ready";
    }
    ValidateGraph();
}

void ShaderGraphTab::ValidateGraph()
{
    validationError_.clear();
    if (GetGraph().Validate(&validationError_))
        status_ = "Shader graph is valid";
    else
        status_ = "Shader graph validation failed";
}

void ShaderGraphTab::GenerateShader(ShaderGraphLanguage language)
{
    ea::string error;
    const ea::string source = GetGraph().Generate(language, &error);
    if (source.empty())
    {
        validationError_ = error;
        status_ = "Shader generation failed";
        return;
    }

    generatedLanguage_ = language;
    generatedSource_ = source;
    validationError_.clear();
    status_ = language == ShaderGraphLanguage::GLSL ? "GLSL generated" : "HLSL generated";
}

void ShaderGraphTab::RenderToolbar()
{
    EditorTheme::PushToolbarColors();
    if (ui::Button(ICON_FA_CIRCLE_CHECK " Validate"))
        ValidateGraph();
    ui::SameLine();
    if (ui::Button(ICON_FA_CODE " Generate GLSL"))
        GenerateShader(ShaderGraphLanguage::GLSL);
    ui::SameLine();
    if (ui::Button(ICON_FA_CODE " Generate HLSL"))
        GenerateShader(ShaderGraphLanguage::HLSL);
    ui::SameLine();
    if (ui::Button(EditorIcons::ResetLabel))
        ResetDemoGraph();
    EditorTheme::PopToolbarColors();
    ui::SameLine();
    ui::TextColored(EditorThemeColors::ToColor(EditorThemeColors::TextMuted), "%s", status_.c_str());
}

void ShaderGraphTab::RenderNodeList(ShaderGraph& graph)
{
    ui::Text("Nodes");
    if (ui::BeginTable("ShaderGraphNodes", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV
            | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY, ImVec2(0, 260)))
    {
        ui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 42.0f);
        ui::TableSetupColumn("Name");
        ui::TableSetupColumn("Kind");
        ui::TableHeadersRow();
        for (const ShaderGraphNode& node : graph.GetNodes())
        {
            ui::TableNextRow();
            ui::TableSetColumnIndex(0);
            ui::PushID(static_cast<int>(node.id));
            ui::Text("%u", node.id);
            ui::TableSetColumnIndex(1);
            if (ui::Selectable(node.name.c_str(), selectedNodeId_ == node.id, ImGuiSelectableFlags_SpanAllColumns))
                selectedNodeId_ = node.id;
            ui::TableSetColumnIndex(2);
            ui::TextUnformatted(GetNodeKindName(node.kind));
            ui::PopID();
        }
        ui::EndTable();
    }

    static const char* nodeKinds[] = {"Constant", "Parameter", "Add", "Multiply", "Lerp", "Texture Sample", "Output"};
    static const char* valueTypes[] = {"Float", "Vector2", "Vector3", "Vector4", "Color", "Bool", "Texture2D"};
    int nodeKind = static_cast<int>(newNodeKind_);
    int valueType = static_cast<int>(newNodeType_);
    ui::SetNextItemWidth(-1.0f);
    if (ui::Combo("##ShaderGraphNodeKind", &nodeKind, nodeKinds, IM_ARRAYSIZE(nodeKinds)))
        newNodeKind_ = static_cast<unsigned>(nodeKind);
    ui::SetNextItemWidth(-1.0f);
    if (ui::Combo("##ShaderGraphValueType", &valueType, valueTypes, IM_ARRAYSIZE(valueTypes)))
        newNodeType_ = static_cast<unsigned>(valueType);
    EditorTheme::PushToolbarColors(true);
    if (ui::Button(EditorIcons::AddLabel, ImVec2(-1.0f, 0.0f)))
    {
        const JSONValue before = CaptureGraph();
        const ShaderGraphNodeKind kind = static_cast<ShaderGraphNodeKind>(newNodeKind_);
        const ShaderGraphValueType type = static_cast<ShaderGraphValueType>(newNodeType_);
        const unsigned id = graph.AddNode("New Node", kind, type);
        if (kind == ShaderGraphNodeKind::Output)
            graph.SetOutputNode(id);
        selectedNodeId_ = id;
        CommitGraphEdit(before, "Added shader graph node");
    }
    EditorTheme::PopToolbarColors();
}

void ShaderGraphTab::RenderNodeInspector(ShaderGraph& graph)
{
    ui::Text("Node Inspector");
    ShaderGraphNode* node = graph.GetNode(selectedNodeId_);
    if (!node)
    {
        ui::TextDisabled("Select a node to edit its metadata.");
        return;
    }

    const JSONValue before = CaptureGraph();
    ui::Text("ID: %u", node->id);
    ui::Text("Kind: %s", GetNodeKindName(node->kind));
    ui::Text("Type: %s", GetValueTypeName(node->valueType));
    ui::InputText("Name", &node->name);
    if (before != CaptureGraph())
        CommitGraphEdit(before, "Renamed shader graph node");

    EditorTheme::PushToolbarColors();
    if (ui::Button(EditorIcons::RemoveLabel))
    {
        const JSONValue removeBefore = CaptureGraph();
        const unsigned removedId = node->id;
        if (graph.RemoveNode(removedId))
        {
            selectedNodeId_ = graph.GetOutputNode() ? graph.GetOutputNode()->id : 0;
            CommitGraphEdit(removeBefore, "Removed shader graph node");
        }
    }

    if (ui::Button(ICON_FA_PALETTE " Add Tint Parameter"))
    {
        const JSONValue parameterBefore = CaptureGraph();
        if (graph.SetParameter({"Tint", ShaderGraphValueType::Color, Variant(Color::WHITE)}))
            CommitGraphEdit(parameterBefore, "Added Tint parameter");
    }
    EditorTheme::PopToolbarColors();
}

void ShaderGraphTab::RenderConnections(const ShaderGraph& graph)
{
    ui::Text("Connections");
    if (graph.GetConnections().empty())
    {
        ui::TextDisabled("No connections");
        return;
    }

    if (ui::BeginTable("ShaderGraphConnections", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV))
    {
        ui::TableSetupColumn("From");
        ui::TableSetupColumn("Pin");
        ui::TableSetupColumn("To");
        ui::TableSetupColumn("Pin");
        ui::TableHeadersRow();
        for (const ShaderGraphConnection& connection : graph.GetConnections())
        {
            ui::TableNextRow();
            ui::TableSetColumnIndex(0);
            if (const ShaderGraphNode* node = graph.GetNode(connection.fromNode))
                ui::TextUnformatted(node->name.c_str());
            ui::TableSetColumnIndex(1);
            ui::TextUnformatted(connection.fromPin.c_str());
            ui::TableSetColumnIndex(2);
            if (const ShaderGraphNode* node = graph.GetNode(connection.toNode))
                ui::TextUnformatted(node->name.c_str());
            ui::TableSetColumnIndex(3);
            ui::TextUnformatted(connection.toPin.c_str());
        }
        ui::EndTable();
    }
}

void ShaderGraphTab::RenderGeneratedSource()
{
    if (generatedSource_.empty())
        return;

    ui::Separator();
    ui::Text("Generated %s", generatedLanguage_ == ShaderGraphLanguage::GLSL ? "GLSL" : "HLSL");
    ui::InputTextMultiline("##ShaderGraphGeneratedSource", &generatedSource_, ImVec2(-1.0f, 220.0f),
        ImGuiInputTextFlags_ReadOnly);
}

void ShaderGraphTab::RenderContent()
{
    ShaderGraph& graph = GetGraph();
    if (ui::BeginTable("ShaderGraphEditorLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
    {
        ui::TableSetupColumn("Graph", ImGuiTableColumnFlags_WidthFixed, 330.0f);
        ui::TableSetupColumn("Inspector");
        ui::TableNextRow();
        ui::TableSetColumnIndex(0);
        RenderNodeList(graph);
        ui::TableSetColumnIndex(1);
        RenderNodeInspector(graph);
        ui::Separator();
        RenderConnections(graph);
        ui::EndTable();
    }

    if (!validationError_.empty())
    {
        EditorTheme::PushDiagnosticText(true);
        ui::Text("Error: %s", validationError_.c_str());
        EditorTheme::PopDiagnosticText();
    }
    RenderGeneratedSource();
}

void ShaderGraphTab::RenderContextMenuItems()
{
    if (ui::MenuItem("Validate shader graph"))
        ValidateGraph();
    if (ui::MenuItem("Reset shader graph template"))
        ResetDemoGraph();
}

void ShaderGraphTab::OnResourceLoaded(const ea::string& resourceName)
{
    resource_ = GetSubsystem<ResourceCache>()->GetResource<ShaderGraphResource>(resourceName);
    selectedNodeId_ = resource_ && resource_->GetGraph().GetOutputNode() ? resource_->GetGraph().GetOutputNode()->id : 0;
    generatedSource_.clear();
    validationError_.clear();
    status_ = resource_ ? "Shader graph loaded" : "Unable to load shader graph";
}

void ShaderGraphTab::OnResourceUnloaded(const ea::string& resourceName)
{
    if (resourceName == GetActiveResourceName())
        resource_.Reset();
}

void ShaderGraphTab::OnActiveResourceChanged(const ea::string& oldResourceName, const ea::string& newResourceName)
{
    if (newResourceName.empty())
    {
        resource_.Reset();
        return;
    }
    resource_ = GetSubsystem<ResourceCache>()->GetResource<ShaderGraphResource>(newResourceName);
    selectedNodeId_ = resource_ && resource_->GetGraph().GetOutputNode() ? resource_->GetGraph().GetOutputNode()->id : 0;
}

void ShaderGraphTab::OnResourceSaved(const ea::string& resourceName)
{
    status_ = Format("Saved {}", resourceName);
}

void ShaderGraphTab::OnResourceShallowSaved(const ea::string& resourceName)
{
    (void)resourceName;
}

} // namespace Urho3D
