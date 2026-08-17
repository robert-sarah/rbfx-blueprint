//
// SPDX-License-Identifier: MIT
//

#include "EditorAutosave.h"

#include "SettingsManager.h"
#include "../Project/EditorTab.h"
#include "../Project/Project.h"

#include <Urho3D/Core/StringUtils.h>
#include <Urho3D/IO/ArchiveSerialization.h>
#include <Urho3D/IO/File.h>
#include <Urho3D/IO/FileSystem.h>
#include <Urho3D/Resource/JSONFile.h>
#include <Urho3D/SystemUI/SystemUI.h>

#include <EASTL/algorithm.h>

namespace Urho3D
{

namespace
{

const char* RecoveryPopupName = "Editor autosave recovery";
const char* RestorePopupName = "Confirm autosave restore";
const char* AutosaveRootName = "Temp/Autosave/";

struct EditorAutosaveSettings
{
    ea::string GetUniqueName() { return "Editor.Recovery:Autosave"; }

    void SerializeInBlock(Archive& archive)
    {
        SerializeOptionalValue(archive, "Enabled", enabled_, true);
        SerializeOptionalValue(archive, "IntervalSeconds", intervalSeconds_, 120u);
        SerializeOptionalValue(archive, "MaxSnapshots", maxSnapshots_, 10u);
    }

    void RenderSettings()
    {
        ui::Checkbox("Enable autosave", &enabled_);

        int interval = static_cast<int>(intervalSeconds_);
        if (ui::SliderInt("Interval (seconds)", &interval, 10, 3600))
            intervalSeconds_ = static_cast<unsigned>(ea::max(interval, 10));

        int maxSnapshots = static_cast<int>(maxSnapshots_);
        if (ui::SliderInt("Maximum snapshots", &maxSnapshots, 1, 100))
            maxSnapshots_ = static_cast<unsigned>(ea::max(maxSnapshots, 1));
    }

    bool enabled_{true};
    unsigned intervalSeconds_{120};
    unsigned maxSnapshots_{10};
};

class EditorAutosaveSettingsPage final : public SimpleSettingsPage<EditorAutosaveSettings>
{
public:
    using UpdateCallback = ea::function<void(const EditorAutosaveSettings&)>;

    EditorAutosaveSettingsPage(Context* context, UpdateCallback callback)
        : SimpleSettingsPage(context)
        , callback_(ea::move(callback))
    {
    }

    void RenderSettings() override
    {
        const EditorAutosaveSettings before = GetValues();
        SimpleSettingsPage::RenderSettings();
        if (before.enabled_ != GetValues().enabled_
            || before.intervalSeconds_ != GetValues().intervalSeconds_
            || before.maxSnapshots_ != GetValues().maxSnapshots_)
        {
            callback_(GetValues());
        }
    }

    void SerializeInBlock(Archive& archive) override
    {
        SimpleSettingsPage::SerializeInBlock(archive);
        if (archive.IsInput())
            callback_(GetValues());
    }

private:
    UpdateCallback callback_;
};

ea::string EnsureTrailingSlash(ea::string path)
{
    if (!path.ends_with("/"))
        path += "/";
    return path;
}

bool IsSafeRelativePath(const ea::string& path)
{
    return !path.empty() && path.find("..") == ea::string::npos && !path.starts_with("/")
        && path.find('\\') == ea::string::npos;
}

} // namespace

EditorAutosave::EditorAutosave(Context* context, Project* project)
    : Object(context)
    , project_(project)
{
    if (project)
    {
        auto settingsPage = MakeShared<EditorAutosaveSettingsPage>(context,
            [this](const EditorAutosaveSettings& settings)
            {
                enabled_ = settings.enabled_;
                SetIntervalSeconds(settings.intervalSeconds_);
                SetMaxSnapshots(settings.maxSnapshots_);
            });
        project->GetSettingsManager()->AddPage(settingsPage);
    }

    RefreshSnapshots();
}

EditorAutosave::~EditorAutosave()
{
}

void EditorAutosave::SetIntervalSeconds(unsigned seconds)
{
    intervalSeconds_ = ea::clamp(seconds, 10u, 3600u);
    timer_.Reset();
}

void EditorAutosave::SetMaxSnapshots(unsigned count)
{
    maxSnapshots_ = ea::clamp(count, 1u, 100u);
    TrimSnapshots();
}

bool EditorAutosave::HasUnsavedWork() const
{
    return project_ && !project_->GetFlags().Test(ProjectFlag::ReadOnly) && project_->HasUnsavedWork();
}

void EditorAutosave::Update()
{
    if (!enabled_ || !project_ || project_->GetFlags().Test(ProjectFlag::ReadOnly))
        return;

    if (!HasUnsavedWork())
        return;

    if (timer_.GetMSec(false) >= intervalSeconds_ * 1000u)
        SaveNow();
}

ea::string EditorAutosave::CreateSnapshotDirectory() const
{
    if (!project_)
        return {};

    const auto fileSystem = GetSubsystem<FileSystem>();
    const ea::string root = EnsureTrailingSlash(project_->GetProjectPath() + AutosaveRootName);
    if (!fileSystem->CreateDirsRecursive(root))
        return {};

    const ea::string timestamp = Time::GetTimeStamp("%Y%m%d_%H%M%S");
    ea::string directory = root + timestamp + "/";
    unsigned suffix = 1;
    while (fileSystem->DirExists(directory))
        directory = root + Format("{}_{}", timestamp, suffix++) + "/";

    return fileSystem->CreateDirsRecursive(directory) ? directory : ea::string{};
}

bool EditorAutosave::WriteSnapshotManifest(const ea::string& directory, const ea::string& timestamp,
    const ea::vector<ea::string>& unsavedItems) const
{
    if (!project_)
        return false;

    JSONFile manifest(context_);
    JSONValue root(JSON_OBJECT);
    root.Set("Version", 1u);
    root.Set("ProjectPath", project_->GetProjectPath());
    root.Set("Timestamp", timestamp);

    JSONValue items(JSON_ARRAY);
    for (const ea::string& item : unsavedItems)
        items.Push(item);
    root.Set("UnsavedItems", ea::move(items));

    JSONValue files(JSON_ARRAY);
    auto fileSystem = GetSubsystem<FileSystem>();
    const ea::string projectJson = project_->GetProjectPath() + "Project.json";
    const ea::string uiIni = project_->GetProjectPath() + "ui.ini";
    if (fileSystem->FileExists(projectJson))
        files.Push("Project.json");
    if (fileSystem->FileExists(uiIni))
        files.Push("ui.ini");
    root.Set("Files", ea::move(files));

    manifest.GetRoot() = ea::move(root);
    return manifest.SaveFile(directory + "manifest.json");
}

bool EditorAutosave::WriteCapturedFiles(const ea::string& directory, ea::vector<ea::string>& capturedFiles) const
{
    if (!project_)
        return false;

    auto fileSystem = GetSubsystem<FileSystem>();
    const ea::string projectPath = project_->GetProjectPath();

    const auto copyIfPresent = [&](const ea::string& relativeName)
    {
        const ea::string source = projectPath + relativeName;
        const ea::string destination = directory + relativeName;
        if (!fileSystem->FileExists(source))
            return true;
        if (!fileSystem->CreateDirsRecursive(GetPath(destination)))
            return false;
        if (!fileSystem->Copy(source, destination))
            return false;
        capturedFiles.push_back(relativeName);
        return true;
    };

    if (!copyIfPresent("Project.json") || !copyIfPresent("ui.ini"))
        return false;

    // Resource tabs can serialize their in-memory state without changing the active project.
    project_->WriteAutosaveSnapshot(directory, capturedFiles);
    return true;
}

bool EditorAutosave::SaveNow()
{
    if (!enabled_ || !HasUnsavedWork())
        return false;

    ea::vector<ea::string> unsavedItems;
    project_->EnumerateUnsavedItems(unsavedItems);
    if (unsavedItems.empty())
        return false;

    const ea::string directory = CreateSnapshotDirectory();
    if (directory.empty())
    {
        URHO3D_LOGWARNING("Cannot create editor autosave directory for project {}", project_->GetProjectPath());
        return false;
    }

    ea::vector<ea::string> capturedFiles;
    auto fileSystem = GetSubsystem<FileSystem>();
    if (!WriteCapturedFiles(directory, capturedFiles))
    {
        fileSystem->RemoveDir(directory, true);
        URHO3D_LOGWARNING("Cannot write editor autosave snapshot {}", directory);
        return false;
    }

    const ea::string timestamp = Time::GetTimeStamp("%Y-%m-%dT%H:%M:%S");
    if (!WriteSnapshotManifest(directory, timestamp, unsavedItems))
    {
        fileSystem->RemoveDir(directory, true);
        URHO3D_LOGWARNING("Cannot write editor autosave manifest {}", directory);
        return false;
    }

    RefreshSnapshots();
    TrimSnapshots();
    timer_.Reset();
    recoveryPending_ = false;
    selectedSnapshot_ = 0;
    URHO3D_LOGINFO("Editor autosave snapshot created: {}", directory);
    return true;
}

void EditorAutosave::RefreshSnapshots()
{
    snapshots_.clear();
    if (!project_)
        return;

    auto fileSystem = GetSubsystem<FileSystem>();
    const ea::string root = EnsureTrailingSlash(project_->GetProjectPath() + AutosaveRootName);
    if (!fileSystem->DirExists(root))
    {
        recoveryPending_ = false;
        return;
    }

    ea::vector<ea::string> directories;
    fileSystem->ScanDir(directories, root, "*", SCAN_DIRS);
    for (const ea::string& directoryName : directories)
    {
        const ea::string directory = EnsureTrailingSlash(root + directoryName);
        const ea::string manifestName = directory + "manifest.json";
        if (!fileSystem->FileExists(manifestName))
            continue;

        JSONFile manifest(context_);
        if (!manifest.LoadFile(manifestName) || !manifest.GetRoot().IsObject())
            continue;

        const JSONValue& rootValue = manifest.GetRoot();
        EditorAutosaveSnapshot snapshot;
        snapshot.directory_ = directory;
        snapshot.timestamp_ = rootValue.Get("Timestamp").GetString(directoryName);
        snapshot.modifiedTime_ = fileSystem->GetLastModifiedTime(manifestName);

        const JSONValue& items = rootValue.Get("UnsavedItems");
        if (items.IsArray())
        {
            for (const JSONValue& item : items.GetArray())
            {
                if (item.IsString())
                    snapshot.unsavedItems_.push_back(item.GetString());
            }
        }
        snapshots_.push_back(ea::move(snapshot));
    }

    ea::sort(snapshots_.begin(), snapshots_.end(), [](const auto& lhs, const auto& rhs)
    {
        return lhs.modifiedTime_ > rhs.modifiedTime_;
    });

    if (selectedSnapshot_ >= snapshots_.size())
        selectedSnapshot_ = snapshots_.empty() ? 0 : static_cast<unsigned>(snapshots_.size() - 1);
    recoveryPending_ = !snapshots_.empty();
}

void EditorAutosave::TrimSnapshots()
{
    if (!project_)
        return;

    auto fileSystem = GetSubsystem<FileSystem>();
    while (snapshots_.size() > maxSnapshots_)
    {
        const EditorAutosaveSnapshot& oldest = snapshots_.back();
        fileSystem->RemoveDir(oldest.directory_, true);
        snapshots_.pop_back();
    }

    if (selectedSnapshot_ >= snapshots_.size())
        selectedSnapshot_ = snapshots_.empty() ? 0 : static_cast<unsigned>(snapshots_.size() - 1);
}

void EditorAutosave::MarkClean()
{
    if (!project_ || project_->HasUnsavedWork())
        return;

    auto fileSystem = GetSubsystem<FileSystem>();
    for (const EditorAutosaveSnapshot& snapshot : snapshots_)
        fileSystem->RemoveDir(snapshot.directory_, true);
    snapshots_.clear();
    recoveryPending_ = false;
    selectedSnapshot_ = 0;
    timer_.Reset();
}

void EditorAutosave::RevealSelectedSnapshot()
{
    if (selectedSnapshot_ >= snapshots_.size())
        return;
    GetSubsystem<FileSystem>()->Reveal(snapshots_[selectedSnapshot_].directory_);
}

bool EditorAutosave::RestoreSelectedSnapshot()
{
    if (!project_ || selectedSnapshot_ >= snapshots_.size())
        return false;

    const EditorAutosaveSnapshot& snapshot = snapshots_[selectedSnapshot_];
    const ea::string manifestName = snapshot.directory_ + "manifest.json";
    JSONFile manifest(context_);
    if (!manifest.LoadFile(manifestName) || !manifest.GetRoot().IsObject())
        return false;

    const JSONValue& files = manifest.GetRoot().Get("Files");
    if (!files.IsArray())
        return false;

    auto fileSystem = GetSubsystem<FileSystem>();
    const ea::string projectPath = project_->GetProjectPath();
    bool restoredAny = false;
    for (const JSONValue& file : files.GetArray())
    {
        if (!file.IsString() || !IsSafeRelativePath(file.GetString()))
            continue;

        const ea::string relativeName = file.GetString();
        const ea::string source = snapshot.directory_ + relativeName;
        const ea::string destination = projectPath + relativeName;
        if (!fileSystem->FileExists(source))
            continue;
        if (!fileSystem->CreateDirsRecursive(GetPath(destination)))
            continue;
        restoredAny = fileSystem->Copy(source, destination) || restoredAny;
    }

    if (restoredAny)
    {
        recoveryPending_ = false;
        restoreConfirmationPending_ = false;
        project_->MarkUnsaved();
        URHO3D_LOGINFO("Editor autosave snapshot restored: {}", snapshot.directory_);
    }
    return restoredAny;
}

void EditorAutosave::DismissRecovery()
{
    recoveryPending_ = false;
    restoreConfirmationPending_ = false;
}

void EditorAutosave::RenderRestoreConfirmation()
{
    if (restoreConfirmationPending_)
    {
        restoreConfirmationPending_ = false;
        ui::OpenPopup(RestorePopupName);
    }

    if (!ui::BeginPopupModal(RestorePopupName, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;

    ui::Text("Restore the selected autosave over the current project files?");
    ui::TextDisabled("The current in-memory state will remain open until resources reload.");
    if (ui::Button("Restore") && RestoreSelectedSnapshot())
        ui::CloseCurrentPopup();
    ui::SameLine();
    if (ui::Button("Cancel") || ui::IsKeyPressed(KEY_ESCAPE))
        ui::CloseCurrentPopup();
    ui::EndPopup();
}

void EditorAutosave::RenderRecoveryDialog()
{
    if (recoveryPending_)
    {
        recoveryPending_ = false;
        ui::OpenPopup(RecoveryPopupName);
    }

    if (ui::BeginPopupModal(RecoveryPopupName, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ui::Text("Unsaved editor snapshots were found after the previous session.");
        ui::TextDisabled("Choose a snapshot to inspect or restore. Files are never overwritten automatically.");
        ui::Separator();

        if (ui::BeginChild("AutosaveSnapshots", ImVec2{520.0f, 170.0f}, true))
        {
            for (unsigned i = 0; i < snapshots_.size(); ++i)
            {
                const EditorAutosaveSnapshot& snapshot = snapshots_[i];
                const bool selected = selectedSnapshot_ == i;
                if (ui::Selectable(Format("{}  ({})", snapshot.timestamp_, snapshot.unsavedItems_.size()).c_str(), selected))
                    selectedSnapshot_ = i;
                if (selected && ui::IsItemHovered())
                    ui::SetTooltip("%s", snapshot.directory_.c_str());
            }
        }
        ui::EndChild();

        if (ui::Button("Restore selected") && selectedSnapshot_ < snapshots_.size())
            restoreConfirmationPending_ = true;
        ui::SameLine();
        if (ui::Button("Reveal folder") && selectedSnapshot_ < snapshots_.size())
            RevealSelectedSnapshot();
        ui::SameLine();
        if (ui::Button("Dismiss") || ui::IsKeyPressed(KEY_ESCAPE))
        {
            DismissRecovery();
            ui::CloseCurrentPopup();
        }
        ui::EndPopup();
    }

    RenderRestoreConfirmation();
}

}
