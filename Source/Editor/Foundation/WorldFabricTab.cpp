#include "WorldFabricTab.h"

#include "../Project/Project.h"

#include <Urho3D/Core/StringUtils.h>
#include <Urho3D/Resource/ResourceCache.h>
#include <Urho3D/SystemUI/SystemUI.h>

namespace Urho3D
{

namespace
{

class WorldFabricSnapshotAction final : public EditorAction
{
public:
    WorldFabricSnapshotAction(WorldFabricTab* tab, const JSONValue& before, const JSONValue& after)
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
    WeakPtr<WorldFabricTab> tab_;
    JSONValue before_;
    JSONValue after_;
};

const WorldFabricNode* FindNode(const WorldFabricGraphResource& resource, const ea::string& key)
{
    const ea::vector<WorldFabricNode> nodes = resource.GetGraph().GetNodes();
    for (const WorldFabricNode& node : nodes)
    {
        if (node.key == key)
            return resource.GetGraph().GetNode(node.id);
    }
    return nullptr;
}

WorldFabricNode* FindNode(WorldFabricGraphResource& resource, const ea::string& key)
{
    return const_cast<WorldFabricNode*>(FindNode(static_cast<const WorldFabricGraphResource&>(resource), key));
}

const char* GetOperationKindName(WorldFabricOperationKind kind)
{
    switch (kind)
    {
    case WorldFabricOperationKind::AddNode: return "AddNode";
    case WorldFabricOperationKind::RemoveNode: return "RemoveNode";
    case WorldFabricOperationKind::AddDependency: return "AddDependency";
    case WorldFabricOperationKind::RemoveDependency: return "RemoveDependency";
    case WorldFabricOperationKind::SetMetadata: return "SetMetadata";
    }
    return "Unknown";
}

} // namespace

void Foundation_WorldFabricTab(Context* context, Project* project)
{
    project->AddTab(MakeShared<WorldFabricTab>(context));
}

WorldFabricTab::WorldFabricTab(Context* context)
    : ResourceEditorTab(context, "World Fabric", "a72d1f4b-4cde-4d85-8c4e-9e2c9f4a8b11",
          EditorTabFlag::OpenByDefault, EditorTabPlacement::DockCenter)
    , preview_(context)
{
    ResetTemplate();
}

bool WorldFabricTab::CanOpenResource(const ResourceFileDescriptor& desc)
{
    return desc.HasObjectType<WorldFabricGraphResource>();
}

WorldFabricGraphResource& WorldFabricTab::GetWorldFabric()
{
    return resource_ ? *resource_ : preview_;
}

const WorldFabricGraphResource& WorldFabricTab::GetWorldFabric() const
{
    return resource_ ? *resource_ : preview_;
}

JSONValue WorldFabricTab::CaptureGraph() const
{
    return GetWorldFabric().ToJSON();
}

void WorldFabricTab::ApplyGraphSnapshot(const JSONValue& snapshot)
{
    ea::string error;
    if (!GetWorldFabric().FromJSON(snapshot, &error))
    {
        status_ = Format("Unable to restore World Fabric graph: {}", error);
        return;
    }
    if (!FindNode(GetWorldFabric(), selectedNodeKey_))
        selectedNodeKey_.clear();
    validationError_.clear();
    status_ = "World Fabric graph restored";
}

void WorldFabricTab::CommitGraphEdit(const JSONValue& before, const ea::string& status)
{
    const JSONValue after = CaptureGraph();
    if (before == after)
        return;
    PushAction<WorldFabricSnapshotAction>(this, before, after);
    status_ = status;
    validationError_.clear();
}

void WorldFabricTab::ResetTemplate()
{
    const JSONValue before = CaptureGraph();
    WorldFabricGraphResource& resource = GetWorldFabric();
    resource.Reset();
    WorldFabricGraph& graph = resource.GetGraph();
    const WorldFabricId asset = graph.AddNode("asset/player", WorldFabricNodeKind::Asset, "Model");
    const WorldFabricId blueprint = graph.AddNode("blueprint/player", WorldFabricNodeKind::Blueprint, "PlayerBlueprint");
    const WorldFabricId entity = graph.AddNode("entity/player", WorldFabricNodeKind::Entity, "PlayerEntity");
    const WorldFabricId render = graph.AddNode("render/player", WorldFabricNodeKind::RenderResource, "Material");
    graph.AddDependency(blueprint, asset, WorldFabricDependencyKind::BuildsFrom, "model");
    graph.AddDependency(entity, blueprint, WorldFabricDependencyKind::Requires, "spawn");
    graph.AddDependency(render, asset, WorldFabricDependencyKind::References, "material-source");
    selectedNodeKey_ = "entity/player";
    if (resource_)
        CommitGraphEdit(before, "Reset World Fabric template");
    else
        status_ = "World Fabric template ready";
}

void WorldFabricTab::SeedReflection()
{
    const JSONValue before = CaptureGraph();
    const unsigned added = GetWorldFabric().SeedObjectReflection();
    CommitGraphEdit(before, Format("Seeded {} reflected rbfx nodes", added));
}

void WorldFabricTab::ValidateGraph()
{
    ea::string error;
    const WorldFabricGraphResource& resource = GetWorldFabric();
    const ea::vector<WorldFabricId> order = resource.GetBuildOrder(&error);
    if (!error.empty())
    {
        validationError_ = error;
        status_ = "World Fabric validation failed";
        return;
    }
    validationError_.clear();
    status_ = Format("World Fabric valid: {} nodes, digest {}", order.size(), resource.ComputeDigest());
}

void WorldFabricTab::AddNode()
{
    if (newNodeKey_.empty())
    {
        validationError_ = "Node key must not be empty";
        return;
    }
    if (FindNode(GetWorldFabric(), newNodeKey_))
    {
        validationError_ = Format("Node '{}' already exists", newNodeKey_);
        return;
    }

    const JSONValue before = CaptureGraph();
    const WorldFabricId id = GetWorldFabric().GetGraph().AddNode(newNodeKey_,
        static_cast<WorldFabricNodeKind>(newNodeKind_), newNodeType_);
    if (id == InvalidWorldFabricId)
    {
        validationError_ = GetWorldFabric().GetGraph().GetLastError();
        return;
    }
    selectedNodeKey_ = newNodeKey_;
    newNodeKey_.clear();
    newNodeType_.clear();
    CommitGraphEdit(before, Format("Added World Fabric node {}", selectedNodeKey_));
}

void WorldFabricTab::RemoveSelectedNode()
{
    WorldFabricNode* node = FindNode(GetWorldFabric(), selectedNodeKey_);
    if (!node)
        return;
    const JSONValue before = CaptureGraph();
    const ea::string removedKey = node->key;
    if (!GetWorldFabric().GetGraph().RemoveNode(node->id))
        return;
    selectedNodeKey_.clear();
    CommitGraphEdit(before, Format("Removed World Fabric node {}", removedKey));
}

void WorldFabricTab::AddDependency()
{
    const ea::string nodeKey = dependencyNodeKey_.empty() ? selectedNodeKey_ : dependencyNodeKey_;
    if (nodeKey.empty() || dependencyTargetKey_.empty())
    {
        validationError_ = "Both dependency node and target are required";
        return;
    }
    WorldFabricNode* node = FindNode(GetWorldFabric(), nodeKey);
    WorldFabricNode* target = FindNode(GetWorldFabric(), dependencyTargetKey_);
    if (!node || !target)
    {
        validationError_ = "Dependency endpoints must reference existing nodes";
        return;
    }

    const JSONValue before = CaptureGraph();
    if (!GetWorldFabric().GetGraph().AddDependency(node->id, target->id,
            static_cast<WorldFabricDependencyKind>(dependencyKind_), dependencyLabel_))
    {
        validationError_ = GetWorldFabric().GetGraph().GetLastError();
        if (validationError_.empty())
            validationError_ = "Dependency already exists or is invalid";
        return;
    }
    dependencyNodeKey_.clear();
    dependencyTargetKey_.clear();
    dependencyLabel_.clear();
    CommitGraphEdit(before, Format("Added dependency {} -> {}", nodeKey, target->key));
}

void WorldFabricTab::RemoveSelectedDependency()
{
    WorldFabricNode* node = FindNode(GetWorldFabric(), selectedNodeKey_);
    if (!node || selectedDependencyLabel_.empty())
        return;
    const ea::vector<WorldFabricDependency> dependencies = GetWorldFabric().GetGraph().GetDependencies(node->id);
    for (const WorldFabricDependency& dependency : dependencies)
    {
        if (dependency.label != selectedDependencyLabel_)
            continue;
        const JSONValue before = CaptureGraph();
        if (GetWorldFabric().GetGraph().RemoveDependency(node->id, dependency.dependency, dependency.kind, dependency.label))
        {
            CommitGraphEdit(before, "Removed World Fabric dependency");
            selectedDependencyLabel_.clear();
        }
        return;
    }
}

void WorldFabricTab::RenderToolbar()
{
    if (ui::Button("Reset Template"))
        ResetTemplate();
    ui::SameLine();
    if (ui::Button("Seed rbfx Reflection"))
        SeedReflection();
    ui::SameLine();
    if (ui::Button("Validate Graph"))
        ValidateGraph();
    ui::SameLine();
    ui::TextUnformatted(status_.c_str());
}

void WorldFabricTab::RenderNodes(WorldFabricGraphResource& resource)
{
    ui::Text("Semantic Nodes");
    if (ui::BeginTable("WorldFabricNodes", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV
            | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY, ImVec2(0, 310)))
    {
        ui::TableSetupColumn("Key");
        ui::TableSetupColumn("Kind");
        ui::TableSetupColumn("Type");
        ui::TableSetupColumn("Metadata");
        ui::TableHeadersRow();
        for (const WorldFabricNode& node : resource.GetGraph().GetNodes())
        {
            ui::TableNextRow();
            ui::TableSetColumnIndex(0);
            ui::PushID(static_cast<int>(node.id));
            if (ui::Selectable(node.key.c_str(), selectedNodeKey_ == node.key, ImGuiSelectableFlags_SpanAllColumns))
                selectedNodeKey_ = node.key;
            ui::TableSetColumnIndex(1);
            ui::TextUnformatted(WorldFabricGraphResource::GetNodeKindName(node.kind));
            ui::TableSetColumnIndex(2);
            ui::TextUnformatted(node.type.c_str());
            ui::TableSetColumnIndex(3);
            ui::Text("%u", node.metadata.size());
            ui::PopID();
        }
        ui::EndTable();
    }

    ui::InputText("New node key", &newNodeKey_);
    ui::InputText("New node type", &newNodeType_);
    static const char* nodeKinds[] = {"Entity", "Component", "Blueprint", "RbScript", "Asset", "SceneCell",
        "RenderResource", "NetworkObject", "AudioBus", "Animation", "Custom"};
    int nodeKind = static_cast<int>(newNodeKind_);
    ui::Combo("New node kind", &nodeKind, nodeKinds, IM_ARRAYSIZE(nodeKinds));
    newNodeKind_ = static_cast<unsigned>(nodeKind);
    if (ui::Button("Add Node"))
        AddNode();
    ui::SameLine();
    if (ui::Button("Remove Selected Node"))
        RemoveSelectedNode();
}

void WorldFabricTab::RenderNodeInspector(WorldFabricGraphResource& resource)
{
    WorldFabricNode* node = FindNode(resource, selectedNodeKey_);
    if (!node)
    {
        ui::TextUnformatted("Select a node to inspect it.");
        return;
    }

    ui::Text("Node Inspector");
    ui::Text("Key: %s", node->key.c_str());
    const JSONValue before = CaptureGraph();
    int kind = static_cast<int>(node->kind);
    static const char* nodeKinds[] = {"Entity", "Component", "Blueprint", "RbScript", "Asset", "SceneCell",
        "RenderResource", "NetworkObject", "AudioBus", "Animation", "Custom"};
    if (ui::Combo("Kind", &kind, nodeKinds, IM_ARRAYSIZE(nodeKinds)))
    {
        node->kind = static_cast<WorldFabricNodeKind>(kind);
        CommitGraphEdit(before, Format("Changed kind for {}", node->key));
    }
    const JSONValue typeBefore = CaptureGraph();
    if (ui::InputText("Type", &node->type))
        CommitGraphEdit(typeBefore, Format("Changed type for {}", node->key));

    ui::Text("Metadata: %u entries", node->metadata.size());
    for (const auto& metadata : node->metadata)
        ui::BulletText("%s = %s", metadata.first.c_str(), metadata.second.ToString().c_str());
}

void WorldFabricTab::RenderDependencies(WorldFabricGraphResource& resource)
{
    ui::Text("Dependencies");
    WorldFabricNode* selected = FindNode(resource, selectedNodeKey_);
    if (selected)
    {
        for (const WorldFabricDependency& dependency : resource.GetGraph().GetDependencies(selected->id))
        {
            const WorldFabricNode* target = resource.GetGraph().GetNode(dependency.dependency);
            const ea::string label = Format("{} -> {} [{}]", selected->key,
                target ? target->key : "missing", dependency.label);
            if (ui::Selectable(label.c_str(), selectedDependencyLabel_ == dependency.label))
                selectedDependencyLabel_ = dependency.label;
        }
    }

    ui::InputText("Dependency node (blank = selected)", &dependencyNodeKey_);
    ui::InputText("Dependency target", &dependencyTargetKey_);
    ui::InputText("Dependency label", &dependencyLabel_);
    static const char* dependencyKinds[] = {"Requires", "Produces", "References", "Replicates", "StreamsWith", "Profiles", "BuildsFrom"};
    int kind = static_cast<int>(dependencyKind_);
    ui::Combo("Dependency kind", &kind, dependencyKinds, IM_ARRAYSIZE(dependencyKinds));
    dependencyKind_ = static_cast<unsigned>(kind);
    if (ui::Button("Add Dependency"))
        AddDependency();
    ui::SameLine();
    if (ui::Button("Remove Selected Dependency"))
        RemoveSelectedDependency();
}

void WorldFabricTab::RenderBuildOrder(const WorldFabricGraphResource& resource)
{
    ea::string error;
    const ea::vector<WorldFabricId> order = resource.GetBuildOrder(&error);
    ui::Text("Deterministic Evaluation Order");
    if (!error.empty())
    {
        ui::TextColored(ImVec4(1.0f, 0.35f, 0.25f, 1.0f), "%s", error.c_str());
        return;
    }
    for (unsigned index = 0; index < order.size(); ++index)
    {
        const WorldFabricNode* node = resource.GetGraph().GetNode(order[index]);
        ui::Text("%u. %s", index + 1, node ? node->key.c_str() : "missing");
    }
}

void WorldFabricTab::RenderImpactAnalysis(const WorldFabricGraphResource& resource)
{
    const WorldFabricNode* root = FindNode(resource, selectedNodeKey_);
    ui::Separator();
    ui::Text("Impact Analysis");
    if (!root)
    {
        ui::TextUnformatted("Select a node to compute its transitive impact.");
        return;
    }

    ea::vector<WorldFabricId> impacted;
    impacted.push_back(root->id);
    for (unsigned index = 0; index < impacted.size(); ++index)
    {
        const ea::vector<WorldFabricDependency> dependents = resource.GetGraph().GetDependents(impacted[index]);
        for (const WorldFabricDependency& edge : dependents)
        {
            if (ea::find(impacted.begin(), impacted.end(), edge.node) == impacted.end())
                impacted.push_back(edge.node);
        }
    }

    ui::Text("Root: %s | Affected nodes: %u", root->key.c_str(), impacted.size());
    for (unsigned index = 0; index < impacted.size(); ++index)
    {
        const WorldFabricNode* node = resource.GetGraph().GetNode(impacted[index]);
        if (node)
            ui::BulletText("%u. %s (%s)", index + 1, node->key.c_str(), WorldFabricGraphResource::GetNodeKindName(node->kind));
    }
}

void WorldFabricTab::RenderSemanticQuery(const WorldFabricGraphResource& resource)
{
    ui::Separator();
    ui::Text("Semantic Query");
    ui::InputText("Key, type or metadata substring", &semanticQuery_);
    unsigned matches = 0;
    for (const WorldFabricNode& node : resource.GetGraph().GetNodes())
    {
        bool match = semanticQuery_.empty() || node.key.find(semanticQuery_) != ea::string::npos
            || node.type.find(semanticQuery_) != ea::string::npos;
        if (!match)
        {
            for (const auto& metadata : node.metadata)
            {
                if (metadata.first.find(semanticQuery_) != ea::string::npos
                    || metadata.second.ToString().find(semanticQuery_) != ea::string::npos)
                {
                    match = true;
                    break;
                }
            }
        }
        if (match)
        {
            ++matches;
            if (ui::Selectable(Format("{} | {}##query", node.key, node.type).c_str(), selectedNodeKey_ == node.key))
                selectedNodeKey_ = node.key;
        }
    }
    ui::Text("Matches: %u", matches);
}

void WorldFabricTab::RenderProfiler(const WorldFabricGraphResource& resource)
{
    profiler_.SetGraph(&GetWorldFabric().GetGraph());
    ui::Separator();
    ui::Text("Correlated World Fabric Profiler");
    ui::Text("Digest: %llu | Samples: %u", profiler_.ComputeDigest(), profiler_.GetAllNodeStats().size());
    if (ui::Button("Reset Correlated Stats"))
    {
        profiler_.Reset();
        status_ = "World Fabric profiler statistics reset";
    }

    const ea::vector<WorldFabricNodeProfile> profiles = profiler_.GetAllNodeStats();
    if (profiles.empty())
    {
        ui::TextUnformatted("No runtime annotations have been recorded for this graph.");
        return;
    }
    if (ui::BeginTable("WorldFabricProfiles", 6, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV
            | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY, ImVec2(0, 220)))
    {
        ui::TableSetupColumn("Node");
        ui::TableSetupColumn("Channel");
        ui::TableSetupColumn("Calls");
        ui::TableSetupColumn("Average ms");
        ui::TableSetupColumn("Min ms");
        ui::TableSetupColumn("Max ms");
        ui::TableHeadersRow();
        for (const WorldFabricNodeProfile& profile : profiles)
        {
            const WorldFabricNode* node = resource.GetGraph().GetNode(profile.node);
            ui::TableNextRow();
            ui::TableSetColumnIndex(0);
            ui::TextUnformatted(node ? node->key.c_str() : profile.key.c_str());
            ui::TableSetColumnIndex(1);
            ui::TextUnformatted(profile.channel.c_str());
            ui::TableSetColumnIndex(2);
            ui::Text("%llu", profile.calls);
            ui::TableSetColumnIndex(3);
            ui::Text("%.3f", profile.GetAverageMilliseconds());
            ui::TableSetColumnIndex(4);
            ui::Text("%.3f", profile.minimumMilliseconds);
            ui::TableSetColumnIndex(5);
            ui::Text("%.3f", profile.maximumMilliseconds);
        }
        ui::EndTable();
    }
}

void WorldFabricTab::RenderCollaboration(const WorldFabricGraphResource& resource)
{
    collaboration_.SetGraph(const_cast<WorldFabricGraph*>(&resource.GetGraph()));
    if (collaboration_.GetClients().empty())
        collaboration_.AddClient(collaborationClientId_);

    ui::Separator();
    ui::Text("World Fabric Collaboration");
    ui::Text("Revision: %llu | Clients: %u | Locks: %u | Operations: %u",
        collaboration_.GetRevision(), collaboration_.GetClients().size(), collaboration_.GetLocks().size(),
        collaboration_.GetHistory().size());

    ui::InputText("Active client", &collaborationClientId_);
    if (collaborationClientId_.empty())
        collaborationClientId_ = "editor";
    ui::SameLine();
    if (ui::Button("Register Active Client"))
    {
        if (!collaboration_.AddClient(collaborationClientId_))
            status_ = collaboration_.GetLastError();
        else
            status_ = Format("Registered collaboration client {}", collaborationClientId_);
    }

    ui::InputText("New client", &newCollaborationClientId_);
    ui::SameLine();
    if (ui::Button("Add Client"))
    {
        if (!collaboration_.AddClient(newCollaborationClientId_))
            status_ = collaboration_.GetLastError();
        else
        {
            status_ = Format("Added collaboration client {}", newCollaborationClientId_);
            newCollaborationClientId_.clear();
        }
    }

    const WorldFabricNode* selected = FindNode(resource, selectedNodeKey_);
    if (selected)
    {
        ui::Text("Selected node: %s", selected->key.c_str());
        if (ui::Button("Lock Selected Node"))
        {
            if (!collaboration_.Lock(selected->id, collaborationClientId_))
                status_ = collaboration_.GetLastError();
            else
                status_ = Format("Locked {} for {}", selected->key, collaborationClientId_);
        }
        ui::SameLine();
        if (ui::Button("Unlock Selected Node"))
        {
            if (!collaboration_.Unlock(selected->id, collaborationClientId_))
                status_ = collaboration_.GetLastError();
            else
                status_ = Format("Unlocked {}", selected->key);
        }
    }
    else
        ui::TextUnformatted("Select a node to manage its collaboration lock.");

    if (!collaboration_.GetLocks().empty())
    {
        ui::Text("Active locks");
        for (const WorldFabricLock& lock : collaboration_.GetLocks())
        {
            const WorldFabricNode* node = resource.GetGraph().GetNode(lock.node);
            ui::BulletText("%s -> %s (revision %llu)", node ? node->key.c_str() : "missing",
                lock.clientId.c_str(), lock.revision);
        }
    }

    ui::Text("Semantic Timeline / operation history");
    if (collaboration_.GetHistory().empty())
    {
        ui::TextUnformatted("No collaboration operations have been submitted for this graph.");
        return;
    }
    if (ui::BeginTable("WorldFabricSemanticTimeline", 5,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
            ImVec2(0, 180)))
    {
        ui::TableSetupColumn("Revision");
        ui::TableSetupColumn("Client");
        ui::TableSetupColumn("Operation");
        ui::TableSetupColumn("Node");
        ui::TableSetupColumn("Target / key");
        ui::TableHeadersRow();
        for (const WorldFabricOperation& operation : collaboration_.GetHistory())
        {
            const WorldFabricNode* node = resource.GetGraph().GetNode(operation.node);
            const WorldFabricNode* dependency = resource.GetGraph().GetNode(operation.dependency);
            ui::TableNextRow();
            ui::TableSetColumnIndex(0);
            ui::Text("%llu", operation.revision);
            ui::TableSetColumnIndex(1);
            ui::TextUnformatted(operation.clientId.c_str());
            ui::TableSetColumnIndex(2);
            ui::TextUnformatted(GetOperationKindName(operation.kind));
            ui::TableSetColumnIndex(3);
            ui::TextUnformatted(node ? node->key.c_str() : operation.key.c_str());
            ui::TableSetColumnIndex(4);
            ui::TextUnformatted(dependency ? dependency->key.c_str() : operation.metadataKey.c_str());
        }
        ui::EndTable();
    }
}

void WorldFabricTab::RenderDeterministicReproduction(const WorldFabricGraphResource& resource)
{
    ui::Separator();
    ui::Text("Deterministic Reproduction");
    ui::Text("Frame: %u | Fixed delta: %.6f s | Capacity: %u | State digest: %llu",
        deterministicSimulation_.GetCurrentFrame(), deterministicSimulation_.GetFixedDelta(),
        deterministicSimulation_.GetCapacity(), deterministicSimulation_.ComputeStateDigest());

    ui::DragFloat("Fixed delta", &deterministicFixedDelta_, 0.0001f, 0.0001f, 1.0f, "%.6f s");
    int capacity = static_cast<int>(deterministicCapacity_);
    if (ui::InputInt("Snapshot capacity", &capacity))
        deterministicCapacity_ = capacity > 2 ? static_cast<unsigned>(capacity) : 2;

    if (ui::Button("Start"))
    {
        deterministicSimulation_.Configure(deterministicFixedDelta_, deterministicCapacity_);
        StringVariantMap initialState;
        initialState["GraphNodeCount"] = Variant(static_cast<int>(resource.GetGraph().GetNodes().size()));
        initialState["SelectedNode"] = Variant(selectedNodeKey_);
        initialState["GraphDigest"] = Variant(Format("{}", resource.ComputeDigest()));
        deterministicSimulation_.Start(initialState);
        deterministicTargetFrame_ = 0;
        deterministicStatus_ = "Deterministic simulation started from the selected World Fabric graph state.";
    }
    ui::SameLine();
    if (ui::Button("Advance One Frame"))
    {
        StringVariantMap input;
        const bool advanced = deterministicSimulation_.Advance(input,
            [](unsigned frame, float, const StringVariantMap&, const StringVariantMap&, StringVariantMap& nextState)
            {
                nextState["Frame"] = Variant(static_cast<int>(frame));
                return true;
            });
        deterministicStatus_ = advanced ? Format("Advanced to deterministic frame {}", deterministicSimulation_.GetCurrentFrame())
                                        : "Start a deterministic simulation before advancing.";
    }

    int targetFrame = static_cast<int>(deterministicTargetFrame_);
    ui::InputInt("Target frame", &targetFrame);
    deterministicTargetFrame_ = targetFrame > 0 ? static_cast<unsigned>(targetFrame) : 0;
    ui::SameLine();
    if (ui::Button("Restore"))
    {
        const bool restored = deterministicSimulation_.Restore(deterministicTargetFrame_);
        deterministicStatus_ = restored ? Format("Restored deterministic frame {}", deterministicTargetFrame_)
                                        : Format("Snapshot for frame {} is not available", deterministicTargetFrame_);
    }
    ui::SameLine();
    if (ui::Button("Replay To"))
    {
        const bool replayed = deterministicSimulation_.ReplayTo(deterministicTargetFrame_,
            [](unsigned frame, float, const StringVariantMap&, const StringVariantMap&, StringVariantMap& nextState)
            {
                nextState["Frame"] = Variant(static_cast<int>(frame));
                return true;
            });
        deterministicStatus_ = replayed ? Format("Replayed deterministically to frame {}", deterministicTargetFrame_)
                                        : "Replay requires retained inputs and a target at or after the current frame.";
    }

    if (!deterministicStatus_.empty())
        ui::TextUnformatted(deterministicStatus_.c_str());
    if (ui::BeginTable("WorldFabricDeterministicSnapshots", 3,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
            ImVec2(0, 180)))
    {
        ui::TableSetupColumn("Frame");
        ui::TableSetupColumn("Digest");
        ui::TableSetupColumn("State entries");
        ui::TableHeadersRow();
        for (const DeterministicSnapshot& snapshot : deterministicSimulation_.GetSnapshots())
        {
            ui::TableNextRow();
            ui::TableSetColumnIndex(0);
            ui::Text("%u", snapshot.frame);
            ui::TableSetColumnIndex(1);
            ui::Text("%llu", snapshot.digest);
            ui::TableSetColumnIndex(2);
            ui::Text("%u", snapshot.state.size());
        }
        ui::EndTable();
    }
}

void WorldFabricTab::RenderCausalDebugger(const WorldFabricGraphResource& resource)
{
    causalDebugger_.Attach(const_cast<WorldFabricGraph*>(&resource.GetGraph()));
    ui::Separator();
    ui::Text("Causal World Fabric Debugger");
    ui::Text("Evidence: %u | History digest: %llu", static_cast<unsigned>(causalDebugger_.GetEvidence().size()),
        causalDebugger_.ComputeDigest());

    const WorldFabricNode* selected = FindNode(resource, selectedNodeKey_);
    if (!selected)
    {
        ui::TextUnformatted("Select a node to inspect its causal history.");
        return;
    }

    ui::Text("Target: %s", selected->key.c_str());
    if (ui::Button("Record Diagnostic Evidence"))
    {
        causalDebugger_.RecordDiagnostic(selected->id, "WorldFabricEditor", "Manual editor diagnostic");
        causalStatus_ = Format("Recorded diagnostic evidence for {}", selected->key);
    }
    if (!causalStatus_.empty())
        ui::TextUnformatted(causalStatus_.c_str());

    const CausalAnalysis analysis = causalDebugger_.Analyze(selected->id);
    if (!analysis.found)
    {
        ui::TextUnformatted(analysis.summary.c_str());
        return;
    }
    ui::Text("%s", analysis.summary.c_str());
    ui::Text("Impacted semantic nodes: %u", static_cast<unsigned>(analysis.impactedNodes.size()));
    for (const WorldFabricId id : analysis.impactedNodes)
    {
        const WorldFabricNode* node = resource.GetGraph().GetNode(id);
        if (node)
            ui::BulletText("%s", node->key.c_str());
    }
    if (ui::BeginTable("WorldFabricCausalChain", 4,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
            ImVec2(0, 180)))
    {
        ui::TableSetupColumn("Sequence");
        ui::TableSetupColumn("Node");
        ui::TableSetupColumn("Source");
        ui::TableSetupColumn("Message");
        ui::TableHeadersRow();
        for (const CausalEvidence& evidence : analysis.chain)
        {
            const WorldFabricNode* node = resource.GetGraph().GetNode(evidence.node);
            ui::TableNextRow();
            ui::TableSetColumnIndex(0);
            ui::Text("%llu", evidence.sequence);
            ui::TableSetColumnIndex(1);
            ui::TextUnformatted(node ? node->key.c_str() : "missing");
            ui::TableSetColumnIndex(2);
            ui::TextUnformatted(evidence.source.c_str());
            ui::TableSetColumnIndex(3);
            ui::TextUnformatted(evidence.message.c_str());
        }
        ui::EndTable();
    }
}

void WorldFabricTab::RenderTimeMachine(const WorldFabricGraphResource& resource)
{
    ui::Separator();
    ui::Text("Universal Deterministic Time Machine");
    ui::Text("Branch: %s | Frame: %u | Generation: %u | Digest: %llu",
        deterministicTimeMachine_.GetCurrentBranch().c_str(), deterministicTimeMachine_.GetCurrentFrame(),
        deterministicTimeMachine_.GetGeneration(), deterministicTimeMachine_.ComputeDigest());
    ui::InputInt("Step delta", &timeMachineStepDelta_);
    if (ui::Button("Start Universal Timeline"))
    {
        StringVariantMap initial;
        initial["score"] = Variant(0);
        initial["GraphDigest"] = Variant(Format("{}", resource.ComputeDigest()));
        initial["SelectedNode"] = Variant(selectedNodeKey_);
        deterministicTimeMachine_.Start(initial);
        status_ = "Universal deterministic timeline started";
    }
    ui::SameLine();
    if (ui::Button("Advance Timeline"))
    {
        StringVariantMap input;
        input["delta"] = Variant(timeMachineStepDelta_);
        const bool advanced = deterministicTimeMachine_.Advance(input,
            [](unsigned, float, const StringVariantMap& inputState, const StringVariantMap& current,
                StringVariantMap& next)
            {
                next = current;
                const auto score = current.find("score");
                const auto delta = inputState.find("delta");
                next["score"] = Variant((score != current.end() ? score->second.GetInt() : 0)
                    + (delta != inputState.end() ? delta->second.GetInt() : 0));
                return true;
            },
            DeterministicTimeMachineDomain::Gameplay, "World Fabric editor step");
        status_ = advanced ? Format("Timeline advanced to frame {}", deterministicTimeMachine_.GetCurrentFrame())
                           : "Start the universal timeline before advancing";
    }
    ui::SameLine();
    if (ui::Button("Create Investigation Branch"))
    {
        const unsigned checkpoint = deterministicTimeMachine_.GetCurrentFrame();
        std::string error;
        const bool created = deterministicTimeMachine_.CreateBranch("investigation", checkpoint ? checkpoint : 1, &error);
        status_ = created ? "Created investigation branch" : Format("Unable to create branch: {}", error);
    }
    ui::SameLine();
    if (ui::Button("Compare Main / Investigation"))
    {
        unsigned frame{};
        DeterministicFrameDifference difference;
        if (deterministicTimeMachine_.FindFirstDivergence("main", "investigation", frame, difference))
            status_ = Format("First divergence at frame {} ({} changed keys)", frame, difference.changedKeys.size());
        else
            status_ = "No divergence found between the retained branches";
    }
    if (ui::BeginTable("WorldFabricUniversalTimeline", 4,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
            ImVec2(0, 180)))
    {
        ui::TableSetupColumn("Frame");
        ui::TableSetupColumn("Generation");
        ui::TableSetupColumn("Digest");
        ui::TableSetupColumn("Label");
        ui::TableHeadersRow();
        for (const DeterministicTimeMachineFrame& frame : deterministicTimeMachine_.GetFrames())
        {
            ui::TableNextRow();
            ui::TableSetColumnIndex(0);
            ui::Text("%u", frame.frame);
            ui::TableSetColumnIndex(1);
            ui::Text("%u", frame.generation);
            ui::TableSetColumnIndex(2);
            ui::Text("%llu", frame.digest);
            ui::TableSetColumnIndex(3);
            ui::TextUnformatted(frame.label.c_str());
        }
        ui::EndTable();
    }
}

void WorldFabricTab::RenderBuildCapsule(const WorldFabricGraphResource& resource)
{
    ui::Separator();
    ui::Text("Semantic Build Capsule");
    ui::Text("Entries: %u | Plugins: %u | Capsule digest: %llu",
        static_cast<unsigned>(semanticBuildCapsule_.GetEntries().size()),
        static_cast<unsigned>(semanticBuildCapsule_.GetPlugins().size()), capsuleDigest_);
    if (ui::Button("Capture Semantic Build Capsule"))
    {
        semanticBuildCapsule_.Clear();
        SemanticBuildCapsuleMetadata metadata;
        metadata.engineRevision = "rbfx-blueprint";
        metadata.toolchain = "CMake/C++17";
        metadata.platform = "Editor";
        metadata.architecture = "native";
        metadata.configuration = "development";
        metadata.worldFabricDigest = resource.ComputeDigest();
        metadata.timeMachineDigest = deterministicTimeMachine_.ComputeDigest();
        semanticBuildCapsule_.SetMetadata(metadata);
        for (const WorldFabricNode& node : resource.GetGraph().GetNodes())
        {
            SemanticCapsuleEntry entry;
            entry.path = node.key.c_str();
            entry.category = WorldFabricGraphResource::GetNodeKindName(node.kind);
            entry.platform = "Any";
            entry.size = node.metadata.size();
            entry.contentDigest = node.id ^ metadata.worldFabricDigest;
            if (entry.contentDigest == 0)
                entry.contentDigest = node.id ? node.id : 1;
            semanticBuildCapsule_.AddEntry(entry);
        }
        semanticBuildCapsule_.AddPlugin({"rbfx-blueprint", "P3", metadata.worldFabricDigest});
        std::string error;
        const bool valid = semanticBuildCapsule_.Validate(&error);
        capsuleDigest_ = valid ? semanticBuildCapsule_.ComputeDigest() : 0;
        capsuleStatus_ = valid ? Format("Captured valid capsule with {} semantic entries", semanticBuildCapsule_.GetEntries().size())
                               : Format("Capsule validation failed: {}", error);
    }
    if (!capsuleStatus_.empty())
        ui::TextUnformatted(capsuleStatus_.c_str());
    const SemanticBuildCapsuleMetadata& metadata = semanticBuildCapsule_.GetMetadata();
    ui::Text("World Fabric digest: %llu | Time Machine digest: %llu", metadata.worldFabricDigest,
        metadata.timeMachineDigest);
    for (const SemanticCapsuleEntry& entry : semanticBuildCapsule_.GetEntries())
        ui::BulletText("%s [%s] digest=%llu", entry.path.c_str(), entry.category.c_str(), entry.contentDigest);
}

void WorldFabricTab::RenderContent()
{
    WorldFabricGraphResource& resource = GetWorldFabric();
    ui::Text("Nodes: %u | Digest: %llu", resource.GetGraph().GetNodes().size(), resource.ComputeDigest());
    if (ui::BeginTable("WorldFabricLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
    {
        ui::TableSetupColumn("Nodes", ImGuiTableColumnFlags_WidthFixed, 560.0f);
        ui::TableSetupColumn("Inspector", ImGuiTableColumnFlags_WidthStretch);
        ui::TableNextRow();
        ui::TableSetColumnIndex(0);
        RenderNodes(resource);
        ui::TableSetColumnIndex(1);
        RenderNodeInspector(resource);
        ui::Separator();
        RenderDependencies(resource);
        ui::EndTable();
    }

    ui::Separator();
    RenderBuildOrder(resource);
    RenderImpactAnalysis(resource);
    RenderSemanticQuery(resource);
    RenderProfiler(resource);
    RenderCollaboration(resource);
    RenderDeterministicReproduction(resource);
    RenderCausalDebugger(resource);
    RenderTimeMachine(resource);
    RenderBuildCapsule(resource);
    if (!validationError_.empty())
        ui::TextColored(ImVec4(1.0f, 0.35f, 0.25f, 1.0f), "Error: %s", validationError_.c_str());
}

void WorldFabricTab::RenderContextMenuItems()
{
    if (ui::MenuItem("Reset World Fabric template"))
        ResetTemplate();
    if (ui::MenuItem("Seed rbfx reflection"))
        SeedReflection();
}

void WorldFabricTab::OnResourceLoaded(const ea::string& resourceName)
{
    resource_ = GetSubsystem<ResourceCache>()->GetResource<WorldFabricGraphResource>(resourceName);
    collaboration_.SetGraph(resource_ ? &resource_->GetGraph() : nullptr);
    collaboration_.AddClient(collaborationClientId_);
    deterministicSimulation_.Clear();
    causalDebugger_.Clear();
    deterministicTimeMachine_.Clear();
    semanticBuildCapsule_.Clear();
    capsuleDigest_ = 0;
    causalStatus_.clear();
    capsuleStatus_.clear();
    selectedNodeKey_.clear();
    selectedDependencyLabel_.clear();
    validationError_.clear();
    status_ = resource_ ? "World Fabric graph loaded" : "Unable to load World Fabric graph";
}

void WorldFabricTab::OnResourceUnloaded(const ea::string& resourceName)
{
    if (resourceName == GetActiveResourceName())
        resource_.Reset();
}

void WorldFabricTab::OnActiveResourceChanged(const ea::string&, const ea::string& newResourceName)
{
    if (newResourceName.empty())
    {
        resource_.Reset();
        return;
    }
    resource_ = GetSubsystem<ResourceCache>()->GetResource<WorldFabricGraphResource>(newResourceName);
    collaboration_.SetGraph(resource_ ? &resource_->GetGraph() : nullptr);
    collaboration_.AddClient(collaborationClientId_);
    deterministicSimulation_.Clear();
    causalDebugger_.Clear();
    deterministicTimeMachine_.Clear();
    semanticBuildCapsule_.Clear();
    capsuleDigest_ = 0;
    causalStatus_.clear();
    capsuleStatus_.clear();
    selectedNodeKey_.clear();
    selectedDependencyLabel_.clear();
    validationError_.clear();
}

void WorldFabricTab::OnResourceSaved(const ea::string& resourceName)
{
    status_ = Format("Saved {}", resourceName);
}

void WorldFabricTab::OnResourceShallowSaved(const ea::string&)
{
}

} // namespace Urho3D
