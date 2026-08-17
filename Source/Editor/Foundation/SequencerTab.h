// SPDX-License-Identifier: MIT

#pragma once

#include "../Project/ResourceEditorTab.h"

#include <Urho3D/Animation/SequencerResource.h>

namespace Urho3D
{

/// Register the production cinematic sequencer editor in a project.
void Foundation_SequencerTab(Context* context, Project* project);

/// Timeline editor for deterministic transform, animation, audio and event tracks.
class SequencerTab : public ResourceEditorTab
{
    URHO3D_OBJECT(SequencerTab, ResourceEditorTab);

public:
    explicit SequencerTab(Context* context);

    /// Implement ResourceEditorTab.
    /// @{
    void RenderContent() override;
    void RenderToolbar() override;
    void RenderContextMenuItems() override;
    bool CanOpenResource(const ResourceFileDescriptor& desc) override;
    bool SupportMultipleResources() override { return false; }
    ea::string GetResourceTitle() override { return "Sequencer"; }
    bool IsUndoSupported() override { return true; }
    /// @}

    /// Apply a complete sequence snapshot for undo/redo.
    void ApplySequencerSnapshot(const JSONValue& snapshot);

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
    Sequencer& GetSequencer();
    const Sequencer& GetSequencer() const;
    JSONValue CaptureSequencer() const;
    void CommitSequencerEdit(const JSONValue& before, const ea::string& status);
    void ResetDemoSequence();
    void ValidateSequence();
    void TogglePlayback();
    void Seek(float time);
    void RenderTrackList(Sequencer& sequencer);
    void RenderTimeline(Sequencer& sequencer);
    void RenderTrackInspector(Sequencer& sequencer);
    void RenderPlaybackEvents(Sequencer& sequencer);

    SharedPtr<SequencerResource> resource_;
    Sequencer previewSequencer_;
    ea::string status_;
    ea::string validationError_;
    ea::string selectedTrack_;
    ea::string newTrackName_{"New Track"};
    ea::string newEventValue_{"Event"};
    float playhead_{};
    float newKeyTime_{0.5f};
    float newNumericValue_{};
    unsigned newTrackType_{};
};

} // namespace Urho3D
