//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <Urho3D/Core/Object.h>
#include <Urho3D/Core/Timer.h>
#include <Urho3D/IO/ScanFlags.h>

#include <EASTL/vector.h>

namespace Urho3D
{

class Project;

/// A recoverable editor snapshot stored below Project/Temp/Autosave.
struct EditorAutosaveSnapshot
{
    ea::string directory_;
    ea::string timestamp_;
    ea::vector<ea::string> unsavedItems_;
    FileTime modifiedTime_{};
};

/// Project-scoped autosave and crash-recovery coordinator.
///
/// Autosaves never call Save() on the active project. Instead, they write a
/// manifest, disk backups and opt-in in-memory tab snapshots to a timestamped
/// directory. Restoring is an explicit user action from the recovery dialog.
class EditorAutosave : public Object
{
    URHO3D_OBJECT(EditorAutosave, Object);

public:
    explicit EditorAutosave(Context* context, Project* project);
    ~EditorAutosave() override;

    /// Update timer-driven autosave state. Safe to call once per frame.
    void Update();
    /// Create a snapshot immediately when there is unsaved editor work.
    bool SaveNow();
    /// Render the stale-snapshot recovery dialog, if one is pending.
    void RenderRecoveryDialog();

    /// Remove snapshots after a normal, explicit project save.
    void MarkClean();
    /// Refresh the snapshot list from disk.
    void RefreshSnapshots();
    /// Reveal the selected snapshot directory in the host file browser.
    void RevealSelectedSnapshot();
    /// Restore the selected snapshot's captured files over the active project.
    bool RestoreSelectedSnapshot();
    /// Dismiss recovery without deleting files.
    void DismissRecovery();

    /// Return settings and current recovery state.
    bool IsEnabled() const { return enabled_; }
    void SetEnabled(bool enabled) { enabled_ = enabled; }
    unsigned GetIntervalSeconds() const { return intervalSeconds_; }
    void SetIntervalSeconds(unsigned seconds);
    unsigned GetMaxSnapshots() const { return maxSnapshots_; }
    void SetMaxSnapshots(unsigned count);
    bool HasPendingRecovery() const { return recoveryPending_; }
    const ea::vector<EditorAutosaveSnapshot>& GetSnapshots() const { return snapshots_; }

private:
    bool HasUnsavedWork() const;
    ea::string CreateSnapshotDirectory() const;
    bool WriteSnapshotManifest(const ea::string& directory, const ea::string& timestamp,
        const ea::vector<ea::string>& unsavedItems) const;
    bool WriteCapturedFiles(const ea::string& directory, ea::vector<ea::string>& capturedFiles) const;
    void TrimSnapshots();
    void RenderRestoreConfirmation();

    WeakPtr<Project> project_;
    Timer timer_;
    ea::vector<EditorAutosaveSnapshot> snapshots_;
    unsigned intervalSeconds_{120};
    unsigned maxSnapshots_{10};
    bool enabled_{true};
    bool recoveryPending_{};
    bool restoreConfirmationPending_{};
    unsigned selectedSnapshot_{};
};

}
