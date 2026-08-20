// SPDX-License-Identifier: MIT

#include "SequencerTab.h"

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

const char* GetTrackTypeName(SequencerTrackType type)
{
    switch (type)
    {
    case SequencerTrackType::Transform: return "Transform";
    case SequencerTrackType::Animation: return "Animation";
    case SequencerTrackType::Audio: return "Audio";
    case SequencerTrackType::Event: return "Event";
    default: return "Unknown";
    }
}

class SequencerSnapshotAction final : public EditorAction
{
public:
    SequencerSnapshotAction(SequencerTab* tab, const JSONValue& before, const JSONValue& after)
        : tab_(tab)
        , before_(before)
        , after_(after)
    {
    }

    void Redo() const override
    {
        if (tab_)
            tab_->ApplySequencerSnapshot(after_);
    }

    void Undo() const override
    {
        if (tab_)
            tab_->ApplySequencerSnapshot(before_);
    }

private:
    WeakPtr<SequencerTab> tab_;
    JSONValue before_;
    JSONValue after_;
};

} // namespace

void Foundation_SequencerTab(Context* context, Project* project)
{
    project->AddTab(MakeShared<SequencerTab>(context));
}

SequencerTab::SequencerTab(Context* context)
    : ResourceEditorTab(context, "Sequencer", "2d865d75-1d5f-4c43-8b32-12b4d9f7b6c8",
          EditorTabFlag::OpenByDefault, EditorTabPlacement::DockCenter)
{
    ResetDemoSequence();
}

bool SequencerTab::CanOpenResource(const ResourceFileDescriptor& desc)
{
    return desc.HasObjectType<SequencerResource>();
}

Sequencer& SequencerTab::GetSequencer()
{
    return resource_ ? resource_->GetSequencer() : previewSequencer_;
}

const Sequencer& SequencerTab::GetSequencer() const
{
    return resource_ ? resource_->GetSequencer() : previewSequencer_;
}

JSONValue SequencerTab::CaptureSequencer() const
{
    if (resource_)
        return resource_->ToJSON();

    SequencerResource temporary(GetContext());
    temporary.SetSequencer(previewSequencer_);
    return temporary.ToJSON();
}

void SequencerTab::ApplySequencerSnapshot(const JSONValue& snapshot)
{
    ea::string error;
    if (resource_)
    {
        if (!resource_->FromJSON(snapshot, &error))
        {
            status_ = Format("Unable to restore sequence: {}", error);
            return;
        }
    }
    else
    {
        SequencerResource temporary(GetContext());
        if (!temporary.FromJSON(snapshot, &error))
        {
            status_ = Format("Unable to restore sequence: {}", error);
            return;
        }
        previewSequencer_ = temporary.GetSequencer();
    }

    playhead_ = GetSequencer().GetPosition();
    if (!GetSequencer().GetTrack(selectedTrack_))
        selectedTrack_ = GetSequencer().GetTracks().empty() ? ea::string{} : GetSequencer().GetTracks().front().name;
    status_ = "Sequencer state restored";
}

void SequencerTab::CommitSequencerEdit(const JSONValue& before, const ea::string& status)
{
    const JSONValue after = CaptureSequencer();
    if (before == after)
        return;

    PushAction<SequencerSnapshotAction>(this, before, after);
    status_ = status;
    validationError_.clear();
}

void SequencerTab::ResetDemoSequence()
{
    Sequencer sequence;
    sequence.SetDuration(8.0f);
    sequence.SetLooping(false);

    ea::string error;
    sequence.AddTrack("Camera", SequencerTrackType::Transform, &error);
    sequence.AddTrack("Character", SequencerTrackType::Animation, &error);
    sequence.AddTrack("Music", SequencerTrackType::Audio, &error);
    sequence.AddTrack("Events", SequencerTrackType::Event, &error);
    sequence.AddKeyframe("Camera", {0.0f, Variant(0.0f)}, &error);
    sequence.AddKeyframe("Camera", {8.0f, Variant(1.0f)}, &error);
    sequence.AddKeyframe("Character", {0.0f, Variant("Idle")}, &error);
    sequence.AddKeyframe("Character", {2.0f, Variant("Run")}, &error);
    sequence.AddKeyframe("Music", {0.0f, Variant("Intro")}, &error);
    sequence.AddKeyframe("Events", {4.0f, Variant("Shot_02")}, &error);

    if (resource_)
    {
        const JSONValue before = CaptureSequencer();
        resource_->SetSequencer(sequence);
        selectedTrack_ = "Camera";
        playhead_ = 0.0f;
        CommitSequencerEdit(before, "Reset sequence to template");
    }
    else
    {
        previewSequencer_ = sequence;
        selectedTrack_ = "Camera";
        playhead_ = 0.0f;
        status_ = "Sequencer template ready";
    }
    ValidateSequence();
}

void SequencerTab::ValidateSequence()
{
    validationError_.clear();
    SequencerResource temporary(GetContext());
    if (temporary.FromJSON(CaptureSequencer(), &validationError_))
        status_ = "Sequencer is valid";
    else
        status_ = "Sequencer validation failed";
}

void SequencerTab::TogglePlayback()
{
    Sequencer& sequencer = GetSequencer();
    if (sequencer.IsPlaying())
    {
        if (sequencer.IsPaused())
        {
            if (sequencer.Resume())
                status_ = "Sequencer resumed";
        }
        else if (sequencer.Pause())
            status_ = "Sequencer paused";
    }
    else if (sequencer.Play())
        status_ = "Sequencer playing";
    else
        status_ = "Sequencer could not start";
}

void SequencerTab::Seek(float time)
{
    Sequencer& sequencer = GetSequencer();
    if (sequencer.Seek(time))
    {
        playhead_ = sequencer.GetPosition();
        status_ = Format("Scrubbed to {:.2f}s", playhead_);
    }
}

void SequencerTab::RenderToolbar()
{
    Sequencer& sequencer = GetSequencer();
    EditorTheme::PushToolbarColors();
    if (ui::Button(EditorIcons::ValidateLabel))
        ValidateSequence();
    ui::SameLine();
    if (ui::Button(sequencer.IsPlaying() && !sequencer.IsPaused() ? ICON_FA_PAUSE " Pause" : ICON_FA_PLAY " Play"))
        TogglePlayback();
    ui::SameLine();
    if (ui::Button(ICON_FA_STOP " Stop"))
    {
        sequencer.Stop();
        playhead_ = sequencer.GetPosition();
        status_ = "Sequencer stopped";
    }
    ui::SameLine();
    if (ui::Button(EditorIcons::ResetLabel))
        ResetDemoSequence();
    EditorTheme::PopToolbarColors();
    ui::SameLine();
    ui::TextColored(EditorThemeColors::ToColor(EditorThemeColors::TextMuted), "%s", status_.c_str());
}

void SequencerTab::RenderTrackList(Sequencer& sequencer)
{
    ui::Text("Tracks");
    if (ui::BeginTable("SequencerTracks", 4,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
            ImVec2(0, 230)))
    {
        ui::TableSetupColumn("Name");
        ui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ui::TableSetupColumn("Muted", ImGuiTableColumnFlags_WidthFixed, 55.0f);
        ui::TableSetupColumn("Keys", ImGuiTableColumnFlags_WidthFixed, 45.0f);
        ui::TableHeadersRow();
        for (const SequencerTrack& track : sequencer.GetTracks())
        {
            ui::TableNextRow();
            ui::TableSetColumnIndex(0);
            ui::PushID(track.name.c_str());
            if (ui::Selectable(track.name.c_str(), selectedTrack_ == track.name, ImGuiSelectableFlags_SpanAllColumns))
                selectedTrack_ = track.name;
            ui::TableSetColumnIndex(1);
            ui::TextUnformatted(GetTrackTypeName(track.type));
            ui::TableSetColumnIndex(2);
            ui::TextUnformatted(track.muted ? "Yes" : "No");
            ui::TableSetColumnIndex(3);
            ui::Text("%u", static_cast<unsigned>(track.keyframes.size()));
            ui::PopID();
        }
        ui::EndTable();
    }

    static const char* trackTypes[] = {"Transform", "Animation", "Audio", "Event"};
    int trackType = static_cast<int>(newTrackType_);
    ui::SetNextItemWidth(-1.0f);
    if (ui::Combo("##SequencerTrackType", &trackType, trackTypes, IM_ARRAYSIZE(trackTypes)))
        newTrackType_ = static_cast<unsigned>(trackType);
    ui::InputText("Name", &newTrackName_);
    if (ui::Button("Add Track", ImVec2(-1.0f, 0.0f)))
    {
        const JSONValue before = CaptureSequencer();
        ea::string error;
        if (sequencer.AddTrack(newTrackName_, static_cast<SequencerTrackType>(newTrackType_), &error))
        {
            selectedTrack_ = newTrackName_;
            CommitSequencerEdit(before, "Added Sequencer track");
        }
        else
            status_ = Format("Unable to add track: {}", error);
    }

    if (!selectedTrack_.empty() && ui::Button("Remove Selected Track", ImVec2(-1.0f, 0.0f)))
    {
        const JSONValue before = CaptureSequencer();
        if (sequencer.RemoveTrack(selectedTrack_))
        {
            selectedTrack_ = sequencer.GetTracks().empty() ? ea::string{} : sequencer.GetTracks().front().name;
            CommitSequencerEdit(before, "Removed Sequencer track");
        }
    }
}

void SequencerTab::RenderTimeline(Sequencer& sequencer)
{
    ui::Text("Timeline");
    const JSONValue before = CaptureSequencer();

    float duration = sequencer.GetDuration();
    if (ui::DragFloat("Duration", &duration, 0.05f, 0.1f, 36000.0f, "%.2fs"))
    {
        float lastKeyframe = 0.1f;
        for (const SequencerTrack& track : sequencer.GetTracks())
            for (const SequencerKeyframe& keyframe : track.keyframes)
                lastKeyframe = ea::max(lastKeyframe, keyframe.time);
        sequencer.SetDuration(ea::max(duration, lastKeyframe));
        playhead_ = ea::min(playhead_, sequencer.GetDuration());
    }

    bool looping = sequencer.IsLooping();
    if (ui::Checkbox("Loop", &looping))
        sequencer.SetLooping(looping);

    float scrub = ea::min(playhead_, sequencer.GetDuration());
    if (ui::SliderFloat("Playhead", &scrub, 0.0f, sequencer.GetDuration(), "%.2fs"))
        Seek(scrub);
    ui::Text("Position: %.2f / %.2f", sequencer.GetPosition(), sequencer.GetDuration());

    if (ui::BeginTable("SequencerTimeline", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV
            | ImGuiTableFlags_Resizable, ImVec2(0, 190)))
    {
        ui::TableSetupColumn("Track");
        ui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ui::TableSetupColumn("Key time");
        ui::TableSetupColumn("Value");
        ui::TableHeadersRow();
        for (const SequencerTrack& track : sequencer.GetTracks())
        {
            for (const SequencerKeyframe& keyframe : track.keyframes)
            {
                ui::TableNextRow();
                ui::TableSetColumnIndex(0);
                ui::TextUnformatted(track.name.c_str());
                ui::TableSetColumnIndex(1);
                ui::TextUnformatted(GetTrackTypeName(track.type));
                ui::TableSetColumnIndex(2);
                ui::Text("%.2f", keyframe.time);
                ui::TableSetColumnIndex(3);
                ui::TextUnformatted(keyframe.value.ToString().c_str());
            }
        }
        ui::EndTable();
    }

    if (before != CaptureSequencer())
        CommitSequencerEdit(before, "Edited Sequencer timeline");
}

void SequencerTab::RenderTrackInspector(Sequencer& sequencer)
{
    ui::Text("Track Inspector");
    SequencerTrack* track = sequencer.GetTrack(selectedTrack_);
    if (!track)
    {
        ui::TextDisabled("Select a track to edit its keyframes.");
        return;
    }

    const JSONValue before = CaptureSequencer();
    ui::Text("%s (%s)", track->name.c_str(), GetTrackTypeName(track->type));
    ui::Checkbox("Muted", &track->muted);

    float removeTime = -1.0f;
    if (ui::BeginTable("SequencerTrackKeyframes", 4,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable, ImVec2(0, 180)))
    {
        ui::TableSetupColumn("Time");
        ui::TableSetupColumn("Value");
        ui::TableSetupColumn("Type");
        ui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 70.0f);
        ui::TableHeadersRow();
        for (const SequencerKeyframe& keyframe : track->keyframes)
        {
            ui::TableNextRow();
            ui::TableSetColumnIndex(0);
            ui::Text("%.2f", keyframe.time);
            ui::TableSetColumnIndex(1);
            ui::TextUnformatted(keyframe.value.ToString().c_str());
            ui::TableSetColumnIndex(2);
            ui::TextUnformatted(keyframe.value.GetTypeName().c_str());
            ui::TableSetColumnIndex(3);
            ui::PushID(static_cast<int>(keyframe.time * 1000.0f));
            if (ui::SmallButton("Remove"))
                removeTime = keyframe.time;
            ui::PopID();
        }
        ui::EndTable();
    }

    if (removeTime >= 0.0f)
        sequencer.RemoveKeyframe(track->name, removeTime);

    ui::DragFloat("Key Time", &newKeyTime_, 0.05f, 0.0f, sequencer.GetDuration(), "%.2fs");
    ui::DragFloat("Numeric Value", &newNumericValue_, 0.05f, -100000.0f, 100000.0f, "%.3f");
    ui::InputText("Event Value", &newEventValue_);
    if (ui::Button("Add Keyframe", ImVec2(-1.0f, 0.0f)))
    {
        SequencerKeyframe keyframe;
        keyframe.time = ea::min(ea::max(newKeyTime_, 0.0f), sequencer.GetDuration());
        keyframe.value = track->type == SequencerTrackType::Event ? Variant(newEventValue_) : Variant(newNumericValue_);
        ea::string error;
        if (sequencer.AddKeyframe(track->name, keyframe, &error))
            status_ = "Added Sequencer keyframe";
        else
            status_ = Format("Unable to add keyframe: {}", error);
    }

    if (before != CaptureSequencer())
        CommitSequencerEdit(before, "Edited Sequencer track");
}

void SequencerTab::RenderPlaybackEvents(Sequencer& sequencer)
{
    const ea::vector<SequencerEvent> events = sequencer.ConsumeEvents();
    if (!events.empty())
    {
        const SequencerEvent& event = events.back();
        status_ = Format("Event {} at {:.2f}s: {}", event.track, event.time, event.value.ToString());
    }
    ui::Text("Playback: %s", sequencer.IsPlaying() ? (sequencer.IsPaused() ? "Paused" : "Playing") : "Stopped");
    ui::Text("Pending events: %u", static_cast<unsigned>(events.size()));
}

void SequencerTab::RenderContent()
{
    Sequencer& sequencer = GetSequencer();
    if (sequencer.IsPlaying())
    {
        const float timeStep = GetSubsystem<Time>() ? GetSubsystem<Time>()->GetTimeStep() : 1.0f / 60.0f;
        sequencer.Update(timeStep > 0.0f ? timeStep : 1.0f / 60.0f);
        playhead_ = sequencer.GetPosition();
    }

    if (ui::BeginTable("SequencerEditorLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
    {
        ui::TableSetupColumn("Tracks", ImGuiTableColumnFlags_WidthFixed, 310.0f);
        ui::TableSetupColumn("Timeline");
        ui::TableNextRow();
        ui::TableSetColumnIndex(0);
        RenderTrackList(sequencer);
        ui::Separator();
        RenderTrackInspector(sequencer);
        ui::TableSetColumnIndex(1);
        RenderTimeline(sequencer);
        ui::Separator();
        RenderPlaybackEvents(sequencer);
        ui::EndTable();
    }

    if (!validationError_.empty())
        ui::TextColored(ImVec4(1.0f, 0.35f, 0.25f, 1.0f), "Error: %s", validationError_.c_str());
}

void SequencerTab::RenderContextMenuItems()
{
    if (ui::MenuItem("Validate sequence"))
        ValidateSequence();
    if (ui::MenuItem("Reset sequence template"))
        ResetDemoSequence();
    if (ui::MenuItem("Play or pause sequence"))
        TogglePlayback();
}

void SequencerTab::OnResourceLoaded(const ea::string& resourceName)
{
    resource_ = GetSubsystem<ResourceCache>()->GetResource<SequencerResource>(resourceName);
    selectedTrack_ = resource_ && !resource_->GetSequencer().GetTracks().empty()
        ? resource_->GetSequencer().GetTracks().front().name : ea::string{};
    playhead_ = 0.0f;
    validationError_.clear();
    status_ = resource_ ? "Sequencer loaded" : "Unable to load Sequencer";
}

void SequencerTab::OnResourceUnloaded(const ea::string& resourceName)
{
    if (resourceName == GetActiveResourceName())
        resource_.Reset();
}

void SequencerTab::OnActiveResourceChanged(const ea::string& oldResourceName, const ea::string& newResourceName)
{
    (void)oldResourceName;
    if (newResourceName.empty())
    {
        resource_.Reset();
        return;
    }

    resource_ = GetSubsystem<ResourceCache>()->GetResource<SequencerResource>(newResourceName);
    selectedTrack_ = resource_ && !resource_->GetSequencer().GetTracks().empty()
        ? resource_->GetSequencer().GetTracks().front().name : ea::string{};
    playhead_ = 0.0f;
}

void SequencerTab::OnResourceSaved(const ea::string& resourceName)
{
    status_ = Format("Saved {}", resourceName);
}

void SequencerTab::OnResourceShallowSaved(const ea::string& resourceName)
{
    (void)resourceName;
}

} // namespace Urho3D
