// SPDX-License-Identifier: MIT

#include "BuildDashboardTab.h"

#include "../Core/EditorIcons.h"
#include "../Core/EditorTheme.h"
#include "../Project/Project.h"

#include <Urho3D/Core/StringUtils.h>
#include <Urho3D/Resource/ResourceCache.h>
#include <Urho3D/SystemUI/SystemUI.h>

namespace Urho3D
{

namespace
{

class BuildDashboardSnapshotAction final : public EditorAction
{
public:
    BuildDashboardSnapshotAction(BuildDashboardTab* tab, const JSONValue& before, const JSONValue& after)
        : tab_(tab)
        , before_(before)
        , after_(after)
    {
    }

    void Redo() const override
    {
        if (tab_)
            tab_->ApplyDashboardSnapshot(after_);
    }

    void Undo() const override
    {
        if (tab_)
            tab_->ApplyDashboardSnapshot(before_);
    }

private:
    WeakPtr<BuildDashboardTab> tab_;
    JSONValue before_;
    JSONValue after_;
};

BuildTask* FindTask(BuildDashboardResource& dashboard, const ea::string& key)
{
    for (BuildTask& task : dashboard.GetTasks())
    {
        if (task.key == key)
            return &task;
    }
    return nullptr;
}

const BuildTask* FindTask(const BuildDashboardResource& dashboard, const ea::string& key)
{
    for (const BuildTask& task : dashboard.GetTasks())
    {
        if (task.key == key)
            return &task;
    }
    return nullptr;
}

ea::string MakeUniqueTaskKey(const BuildDashboardResource& dashboard)
{
    for (unsigned index = 1; index < 10000; ++index)
    {
        const ea::string candidate = Format("Task{}", index);
        if (!FindTask(dashboard, candidate))
            return candidate;
    }
    return "Task";
}

} // namespace

void Foundation_BuildDashboardTab(Context* context, Project* project)
{
    project->AddTab(MakeShared<BuildDashboardTab>(context));
}

BuildDashboardTab::BuildDashboardTab(Context* context)
    : ResourceEditorTab(context, "Build Dashboard", "b17d7a03-6d3c-4d01-bf7c-1a3d2bd8a2b1",
          EditorTabFlag::OpenByDefault, EditorTabPlacement::DockCenter)
    , preview_(context)
{
    ResetTemplate();
}

bool BuildDashboardTab::CanOpenResource(const ResourceFileDescriptor& desc)
{
    return desc.HasObjectType<BuildDashboardResource>();
}

BuildDashboardResource& BuildDashboardTab::GetDashboard()
{
    return resource_ ? *resource_ : preview_;
}

const BuildDashboardResource& BuildDashboardTab::GetDashboard() const
{
    return resource_ ? *resource_ : preview_;
}

JSONValue BuildDashboardTab::CaptureDashboard() const
{
    return GetDashboard().ToJSON();
}

void BuildDashboardTab::ApplyDashboardSnapshot(const JSONValue& snapshot)
{
    ea::string error;
    if (!GetDashboard().FromJSON(snapshot, &error))
    {
        status_ = Format("Unable to restore build dashboard: {}", error);
        return;
    }
    if (!FindTask(GetDashboard(), selectedTaskKey_))
        selectedTaskKey_.clear();
    validationError_.clear();
    status_ = "Build dashboard state restored";
}

void BuildDashboardTab::CommitDashboardEdit(const JSONValue& before, const ea::string& status)
{
    const JSONValue after = CaptureDashboard();
    if (before == after)
        return;
    PushAction<BuildDashboardSnapshotAction>(this, before, after);
    status_ = status;
    validationError_.clear();
}

void BuildDashboardTab::ResetTemplate()
{
    const JSONValue before = CaptureDashboard();
    ea::vector<BuildTask> tasks;

    BuildTask import;
    import.key = "ImportAssets";
    import.kind = BuildTaskKind::ImportAsset;
    import.metadata["source"] = Variant("Assets");
    tasks.push_back(import);

    BuildTask shader;
    shader.key = "CompileShaders";
    shader.kind = BuildTaskKind::CompileShader;
    shader.metadata["source"] = Variant("Assets/Shaders");
    shader.dependencies.push_back(import.key);
    tasks.push_back(shader);

    BuildTask script;
    script.key = "CompileScripts";
    script.kind = BuildTaskKind::CompileScript;
    script.metadata["source"] = Variant("Scripts");
    tasks.push_back(script);

    BuildTask vfx;
    vfx.key = "CookVFX";
    vfx.kind = BuildTaskKind::CookVFX;
    vfx.metadata["source"] = Variant("Assets/VFX");
    vfx.dependencies.push_back(import.key);
    tasks.push_back(vfx);

    BuildTask package;
    package.key = "PackageGame";
    package.kind = BuildTaskKind::BuildPackage;
    package.metadata["target"] = Variant("Desktop");
    package.dependencies = {shader.key, script.key, vfx.key};
    tasks.push_back(package);

    GetDashboard().SetPlatform("Linux");
    GetDashboard().SetConfiguration("Debug");
    GetDashboard().SetTasks(tasks);
    selectedTaskKey_ = package.key;
    if (resource_)
        CommitDashboardEdit(before, "Reset build dashboard template");
    else
        status_ = "Build dashboard template ready";
}

void BuildDashboardTab::AddTask()
{
    BuildDashboardResource& dashboard = GetDashboard();
    const JSONValue before = CaptureDashboard();
    const ea::string key = newTaskKey_.empty() ? MakeUniqueTaskKey(dashboard) : newTaskKey_;
    if (FindTask(dashboard, key))
    {
        validationError_ = Format("A task named '{}' already exists", key);
        return;
    }

    BuildTask task;
    task.key = key;
    task.kind = static_cast<BuildTaskKind>(newTaskKind_);
    dashboard.GetTasks().push_back(task);
    selectedTaskKey_ = key;
    newTaskKey_.clear();
    CommitDashboardEdit(before, Format("Added build task {}", key));
}

void BuildDashboardTab::RemoveSelectedTask()
{
    if (selectedTaskKey_.empty())
        return;

    BuildDashboardResource& dashboard = GetDashboard();
    const JSONValue before = CaptureDashboard();
    bool removed = false;
    auto& tasks = dashboard.GetTasks();
    for (auto it = tasks.begin(); it != tasks.end(); ++it)
    {
        if (it->key == selectedTaskKey_)
        {
            tasks.erase(it);
            removed = true;
            break;
        }
    }
    if (!removed)
        return;

    for (BuildTask& task : tasks)
        task.dependencies.erase(ea::remove(task.dependencies.begin(), task.dependencies.end(), selectedTaskKey_), task.dependencies.end());
    status_ = Format("Removed build task {}", selectedTaskKey_);
    selectedTaskKey_.clear();
    CommitDashboardEdit(before, status_);
}

void BuildDashboardTab::ValidateGraph()
{
    ea::string error;
    const BuildDashboardResource& dashboard = GetDashboard();
    const ea::vector<ea::string> order = dashboard.GetBuildOrder(&error);
    if (!error.empty())
    {
        validationError_ = error;
        status_ = "Build graph validation failed";
        return;
    }
    validationError_.clear();
    status_ = Format("Build graph valid: {} tasks, digest {}", order.size(), dashboard.ComputeDigest());
}

void BuildDashboardTab::RenderToolbar()
{
    EditorThemeUI::PushToolbarColors();
    if (ui::Button(EditorIcons::ResetLabel))
        ResetTemplate();
    ui::SameLine();
    if (ui::Button(ICON_FA_DIAGRAM_PROJECT " Validate Graph"))
        ValidateGraph();
    EditorThemeUI::PopToolbarColors();
    ui::SameLine();
    ui::TextColored(EditorThemeColors::ToColor(EditorThemeColors::TextMuted), "%s", status_.c_str());
}

void BuildDashboardTab::RenderTasks(BuildDashboardResource& dashboard)
{
    ui::Text("Tasks");
    if (ui::BeginTable("BuildDashboardTasks", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV
            | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY, ImVec2(0, 300)))
    {
        ui::TableSetupColumn("Task");
        ui::TableSetupColumn("Kind");
        ui::TableSetupColumn("Dependencies");
        ui::TableSetupColumn("Metadata");
        ui::TableHeadersRow();
        for (const BuildTask& task : dashboard.GetTasks())
        {
            ui::TableNextRow();
            ui::TableSetColumnIndex(0);
            ui::PushID(task.key.c_str());
            if (ui::Selectable(task.key.c_str(), selectedTaskKey_ == task.key, ImGuiSelectableFlags_SpanAllColumns))
                selectedTaskKey_ = task.key;
            ui::TableSetColumnIndex(1);
            ui::TextUnformatted(BuildDashboardResource::GetBuildTaskKindName(task.kind));
            ui::TableSetColumnIndex(2);
            ui::Text("%u", task.dependencies.size());
            ui::TableSetColumnIndex(3);
            ui::Text("%u", task.metadata.size());
            ui::PopID();
        }
        ui::EndTable();
    }

    ui::Separator();
    ui::InputText("New task key", &newTaskKey_);
    static const char* taskKinds[] = {"ImportAsset", "CompileShader", "CompileScript", "CookVFX", "BuildPackage", "Custom"};
    int taskKind = static_cast<int>(newTaskKind_);
    ui::Combo("New task kind", &taskKind, taskKinds, IM_ARRAYSIZE(taskKinds));
    newTaskKind_ = static_cast<unsigned>(taskKind);
    EditorThemeUI::PushToolbarColors(true);
    if (ui::Button(ICON_FA_PLUS " Add Task"))
        AddTask();
    ui::SameLine();
    if (ui::Button(EditorIcons::RemoveLabel))
        RemoveSelectedTask();
    EditorThemeUI::PopToolbarColors();
}

void BuildDashboardTab::RenderTaskInspector(BuildDashboardResource& dashboard)
{
    BuildTask* task = FindTask(dashboard, selectedTaskKey_);
    if (!task)
    {
        ui::TextUnformatted("Select a task to inspect it.");
        return;
    }

    ui::Text("Task Inspector");
    ui::Text("Key: %s", task->key.c_str());
    const JSONValue before = CaptureDashboard();
    static const char* taskKinds[] = {"ImportAsset", "CompileShader", "CompileScript", "CookVFX", "BuildPackage", "Custom"};
    int taskKind = static_cast<int>(task->kind);
    if (ui::Combo("Kind", &taskKind, taskKinds, IM_ARRAYSIZE(taskKinds)))
    {
        task->kind = static_cast<BuildTaskKind>(taskKind);
        CommitDashboardEdit(before, Format("Changed task kind for {}", task->key));
    }

    ui::Text("Dependencies");
    for (const ea::string& dependency : task->dependencies)
        ui::BulletText("%s", dependency.c_str());
    ui::InputText("Dependency key", &dependencyInput_);
    EditorThemeUI::PushToolbarColors();
    if (ui::Button(ICON_FA_LINK " Add Dependency") && !dependencyInput_.empty())
    {
        const JSONValue dependencyBefore = CaptureDashboard();
        if (dependencyInput_ == task->key || !FindTask(dashboard, dependencyInput_))
            validationError_ = "Dependency must reference another existing task";
        else if (ea::find(task->dependencies.begin(), task->dependencies.end(), dependencyInput_) != task->dependencies.end())
            validationError_ = "Dependency already exists";
        else
        {
            task->dependencies.push_back(dependencyInput_);
            dependencyInput_.clear();
            CommitDashboardEdit(dependencyBefore, Format("Added dependency to {}", task->key));
        }
    }
    EditorThemeUI::PopToolbarColors();

    ui::Separator();
    ui::Text("Metadata: %u entries", task->metadata.size());
    for (const auto& metadata : task->metadata)
        ui::BulletText("%s = %s", metadata.first.c_str(), metadata.second.ToString().c_str());
}

void BuildDashboardTab::RenderBuildOrder(const BuildDashboardResource& dashboard)
{
    ea::string error;
    const ea::vector<ea::string> order = dashboard.GetBuildOrder(&error);
    ui::Text("Deterministic Build Order");
    if (!error.empty())
    {
        EditorThemeUI::PushDiagnosticText(true);
        ui::Text("%s", error.c_str());
        EditorThemeUI::PopDiagnosticText();
        return;
    }
    for (unsigned index = 0; index < order.size(); ++index)
        ui::Text("%u. %s", index + 1, order[index].c_str());
}

void BuildDashboardTab::RenderContent()
{
    BuildDashboardResource& dashboard = GetDashboard();
    ui::Text("Platform: %s", dashboard.GetPlatform().c_str());
    ui::SameLine();
    ui::Text("Configuration: %s", dashboard.GetConfiguration().c_str());

    if (ui::BeginTable("BuildDashboardLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
    {
        ui::TableSetupColumn("Tasks", ImGuiTableColumnFlags_WidthFixed, 500.0f);
        ui::TableSetupColumn("Inspector", ImGuiTableColumnFlags_WidthStretch);
        ui::TableNextRow();
        ui::TableSetColumnIndex(0);
        RenderTasks(dashboard);
        ui::TableSetColumnIndex(1);
        RenderTaskInspector(dashboard);
        ui::EndTable();
    }

    ui::Separator();
    RenderBuildOrder(dashboard);
    if (!validationError_.empty())
    {
        EditorThemeUI::PushDiagnosticText(true);
        ui::Text("Error: %s", validationError_.c_str());
        EditorThemeUI::PopDiagnosticText();
    }
}

void BuildDashboardTab::RenderContextMenuItems()
{
    if (ui::MenuItem("Reset build dashboard template"))
        ResetTemplate();
}

void BuildDashboardTab::OnResourceLoaded(const ea::string& resourceName)
{
    resource_ = GetSubsystem<ResourceCache>()->GetResource<BuildDashboardResource>(resourceName);
    selectedTaskKey_.clear();
    validationError_.clear();
    status_ = resource_ ? "Build dashboard loaded" : "Unable to load build dashboard";
}

void BuildDashboardTab::OnResourceUnloaded(const ea::string& resourceName)
{
    if (resourceName == GetActiveResourceName())
        resource_.Reset();
}

void BuildDashboardTab::OnActiveResourceChanged(const ea::string&, const ea::string& newResourceName)
{
    if (newResourceName.empty())
    {
        resource_.Reset();
        return;
    }
    resource_ = GetSubsystem<ResourceCache>()->GetResource<BuildDashboardResource>(newResourceName);
    selectedTaskKey_.clear();
    validationError_.clear();
}

void BuildDashboardTab::OnResourceSaved(const ea::string& resourceName)
{
    status_ = Format("Saved {}", resourceName);
}

void BuildDashboardTab::OnResourceShallowSaved(const ea::string&)
{
}

} // namespace Urho3D
