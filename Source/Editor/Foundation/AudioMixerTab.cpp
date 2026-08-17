// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#include "AudioMixerTab.h"

#include "../Project/Project.h"

#include <Urho3D/Core/StringUtils.h>
#include <Urho3D/Resource/ResourceCache.h>
#include <Urho3D/SystemUI/SystemUI.h>

namespace Urho3D
{

namespace
{

const char* GetDspTypeName(AudioDspType type)
{
    switch (type)
    {
    case AudioDspType::LowPass: return "Low Pass";
    case AudioDspType::HighPass: return "High Pass";
    case AudioDspType::Equalizer: return "Equalizer";
    case AudioDspType::Compressor: return "Compressor";
    case AudioDspType::Reverb: return "Reverb";
    case AudioDspType::Delay: return "Delay";
    default: return "Unknown";
    }
}

class AudioMixerSnapshotAction final : public EditorAction
{
public:
    AudioMixerSnapshotAction(AudioMixerTab* tab, const JSONValue& before, const JSONValue& after)
        : tab_(tab)
        , before_(before)
        , after_(after)
    {
    }

    void Redo() const override
    {
        if (tab_)
            tab_->ApplyMixerSnapshot(after_);
    }

    void Undo() const override
    {
        if (tab_)
            tab_->ApplyMixerSnapshot(before_);
    }

private:
    WeakPtr<AudioMixerTab> tab_;
    JSONValue before_;
    JSONValue after_;
};

ea::string MakeUniqueBusName(const AudioMixer& mixer)
{
    for (unsigned index = 1; index < 10000; ++index)
    {
        const ea::string candidate = Format("Bus{}", index);
        if (!mixer.GetBus(candidate))
            return candidate;
    }
    return "Bus";
}

ea::string MakeUniqueVoiceId(const AudioMixer& mixer)
{
    for (unsigned index = 1; index < 10000; ++index)
    {
        const ea::string candidate = Format("Voice{}", index);
        if (!mixer.GetVoice(candidate))
            return candidate;
    }
    return "Voice";
}

} // namespace

void Foundation_AudioMixerTab(Context* context, Project* project)
{
    project->AddTab(MakeShared<AudioMixerTab>(context));
}

AudioMixerTab::AudioMixerTab(Context* context)
    : ResourceEditorTab(context, "Audio Mixer", "3e6d6aa6-0f73-4bc8-bca1-123456789abc",
          EditorTabFlag::OpenByDefault, EditorTabPlacement::DockCenter)
{
    ResetDemoMixer();
}

bool AudioMixerTab::CanOpenResource(const ResourceFileDescriptor& desc)
{
    return desc.HasObjectType<AudioMixerResource>();
}

AudioMixer& AudioMixerTab::GetMixer()
{
    return resource_ ? resource_->GetMixer() : previewMixer_;
}

const AudioMixer& AudioMixerTab::GetMixer() const
{
    return resource_ ? resource_->GetMixer() : previewMixer_;
}

JSONValue AudioMixerTab::CaptureMixer() const
{
    if (resource_)
        return resource_->ToJSON();

    AudioMixerResource temporary(GetContext());
    temporary.SetMixer(previewMixer_);
    return temporary.ToJSON();
}

void AudioMixerTab::ApplyMixerSnapshot(const JSONValue& snapshot)
{
    ea::string error;
    if (resource_)
    {
        if (!resource_->FromJSON(snapshot, &error))
        {
            status_ = Format("Unable to restore audio mixer: {}", error);
            return;
        }
    }
    else
    {
        AudioMixerResource temporary(GetContext());
        if (!temporary.FromJSON(snapshot, &error))
        {
            status_ = Format("Unable to restore audio mixer: {}", error);
            return;
        }
        previewMixer_ = temporary.GetMixer();
    }
    if (!GetMixer().GetBus(selectedBus_))
        selectedBus_ = GetMixer().GetBus("Master") ? "Master" : ea::string{};
    status_ = "Audio mixer state restored";
}

void AudioMixerTab::CommitMixerEdit(const JSONValue& before, const ea::string& status)
{
    const JSONValue after = CaptureMixer();
    if (before == after)
        return;
    PushAction<AudioMixerSnapshotAction>(this, before, after);
    status_ = status;
    validationError_.clear();
}

void AudioMixerTab::ResetDemoMixer()
{
    AudioMixer mixer;
    mixer.AddBus({"Master", {}, 1.0f, false, false, {}});
    mixer.AddBus({"Music", "Master", 0.85f, false, false, {}});
    mixer.AddBus({"SFX", "Master", 0.75f, false, false, {}});
    mixer.AddEffect("SFX", {AudioDspType::Compressor, true, 0.8f, 2.0f, 0.1f});
    mixer.AddVoice({"PreviewVoice", "SFX", 1.0f, 0.0f, 1.0f, 1.0f, true});

    if (resource_)
    {
        const JSONValue before = CaptureMixer();
        resource_->SetMixer(mixer);
        selectedBus_ = "Master";
        CommitMixerEdit(before, "Reset audio mixer template");
    }
    else
    {
        previewMixer_ = mixer;
        selectedBus_ = "Master";
        status_ = "Audio mixer template ready";
    }
}

void AudioMixerTab::RenderToolbar()
{
    if (ui::Button("Reset Template"))
        ResetDemoMixer();
    ui::SameLine();
    if (ui::Button("Validate"))
    {
        validationError_.clear();
        if (!GetMixer().GetBus("Master"))
            validationError_ = "Audio mixer requires a Master bus";
        else
            status_ = "Audio mixer is valid";
    }
    ui::SameLine();
    ui::Text("%s", status_.c_str());
}

void AudioMixerTab::RenderBuses(AudioMixer& mixer)
{
    ui::Text("Buses");
    if (ui::BeginTable("AudioMixerBuses", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV
            | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY, ImVec2(0, 250)))
    {
        ui::TableSetupColumn("Bus");
        ui::TableSetupColumn("Parent");
        ui::TableSetupColumn("Volume");
        ui::TableSetupColumn("State");
        ui::TableSetupColumn("Peak");
        ui::TableHeadersRow();
        for (const AudioBus& bus : mixer.GetBuses())
        {
            ui::TableNextRow();
            ui::TableSetColumnIndex(0);
            ui::PushID(bus.name.c_str());
            if (ui::Selectable(bus.name.c_str(), selectedBus_ == bus.name, ImGuiSelectableFlags_SpanAllColumns))
                selectedBus_ = bus.name;
            ui::TableSetColumnIndex(1);
            ui::TextUnformatted(bus.parent.c_str());
            ui::TableSetColumnIndex(2);
            ui::Text("%.2f", bus.volume);
            ui::TableSetColumnIndex(3);
            ui::TextUnformatted(bus.muted ? "Muted" : (bus.soloed ? "Solo" : "Active"));
            ui::TableSetColumnIndex(4);
            ui::Text("%.3f", mixer.GetBusMeter(bus.name).peak);
            ui::PopID();
        }
        ui::EndTable();
    }

    if (ui::Button("Add Bus"))
    {
        const JSONValue before = CaptureMixer();
        const ea::string name = MakeUniqueBusName(mixer);
        if (mixer.AddBus({name, "Master", 1.0f, false, false, {}}))
        {
            selectedBus_ = name;
            CommitMixerEdit(before, Format("Added bus {}", name));
        }
    }
}

void AudioMixerTab::RenderBusInspector(AudioMixer& mixer)
{
    AudioBus* bus = mixer.GetBus(selectedBus_);
    ui::Text("Bus Inspector");
    if (!bus)
    {
        ui::TextDisabled("Select a bus to edit its routing and DSP chain.");
        return;
    }

    const JSONValue before = CaptureMixer();
    ui::Text("Name: %s", bus->name.c_str());
    ui::Text("Parent: %s", bus->parent.empty() ? "<root>" : bus->parent.c_str());
    ui::SliderFloat("Volume", &bus->volume, 0.0f, 2.0f, "%.2f");
    ui::Checkbox("Muted", &bus->muted);
    ui::Checkbox("Solo", &bus->soloed);
    if (before != CaptureMixer())
        CommitMixerEdit(before, Format("Edited bus {}", bus->name));

    ui::Separator();
    ui::Text("DSP Effects");
    static const char* effectTypes[] = {"Low Pass", "High Pass", "Equalizer", "Compressor", "Reverb", "Delay"};
    int effectType = static_cast<int>(newEffectType_);
    ui::Combo("##AudioMixerEffectType", &effectType, effectTypes, IM_ARRAYSIZE(effectTypes));
    newEffectType_ = static_cast<unsigned>(effectType);
    if (ui::Button("Add Effect"))
    {
        const JSONValue effectBefore = CaptureMixer();
        if (mixer.AddEffect(bus->name, {static_cast<AudioDspType>(newEffectType_), true, 1.0f, 0.0f, 0.0f}))
            CommitMixerEdit(effectBefore, Format("Added {} effect", GetDspTypeName(static_cast<AudioDspType>(newEffectType_))));
    }
    for (unsigned index = 0; index < bus->effects.size(); ++index)
    {
        const AudioDspEffect& effect = bus->effects[index];
        ui::BulletText("%u: %s, mix %.2f%s", index + 1, GetDspTypeName(effect.type), effect.mix,
            effect.enabled ? "" : " (disabled)");
    }
}

void AudioMixerTab::RenderVoices(const AudioMixer& mixer)
{
    ui::Text("Voices");
    if (ui::BeginTable("AudioMixerVoices", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV))
    {
        ui::TableSetupColumn("Voice");
        ui::TableSetupColumn("Bus");
        ui::TableSetupColumn("Volume");
        ui::TableSetupColumn("Pan");
        ui::TableSetupColumn("State");
        ui::TableHeadersRow();
        for (const AudioVoice& voice : mixer.GetVoices())
        {
            ui::TableNextRow();
            ui::TableSetColumnIndex(0);
            if (ui::Selectable(voice.id.c_str(), selectedVoice_ == voice.id, ImGuiSelectableFlags_SpanAllColumns))
                selectedVoice_ = voice.id;
            ui::TableSetColumnIndex(1);
            ui::TextUnformatted(voice.bus.c_str());
            ui::TableSetColumnIndex(2);
            ui::Text("%.2f", voice.volume);
            ui::TableSetColumnIndex(3);
            ui::Text("%.2f", voice.pan);
            ui::TableSetColumnIndex(4);
            ui::TextUnformatted(voice.playing ? "Playing" : "Stopped");
        }
        ui::EndTable();
    }

    if (ui::Button("Add Preview Voice"))
    {
        AudioMixer& mutableMixer = const_cast<AudioMixer&>(mixer);
        const JSONValue before = CaptureMixer();
        const ea::string id = MakeUniqueVoiceId(mutableMixer);
        if (mutableMixer.AddVoice({id, selectedBus_.empty() ? "Master" : selectedBus_, 1.0f, 0.0f, 1.0f, 1.0f, true}))
        {
            selectedVoice_ = id;
            CommitMixerEdit(before, Format("Added voice {}", id));
        }
    }

    if (AudioVoice* voice = const_cast<AudioMixer&>(mixer).GetVoice(selectedVoice_))
    {
        const JSONValue before = CaptureMixer();
        ui::SliderFloat("Voice Volume", &voice->volume, 0.0f, 2.0f, "%.2f");
        ui::SliderFloat("Voice Pan", &voice->pan, -1.0f, 1.0f, "%.2f");
        ui::SliderFloat("Voice Pitch", &voice->pitch, 0.01f, 3.0f, "%.2f");
        ui::Checkbox("Playing", &voice->playing);
        if (before != CaptureMixer())
            CommitMixerEdit(before, Format("Edited voice {}", voice->id));
    }
}

void AudioMixerTab::RenderMeters(const AudioMixer& mixer)
{
    ui::Separator();
    ui::Text("Meters");
    for (const AudioBus& bus : mixer.GetBuses())
    {
        const AudioMeter meter = mixer.GetBusMeter(bus.name);
        ui::Text("%-16s RMS %.3f  Peak %.3f  Voices %u", bus.name.c_str(), meter.rms, meter.peak,
            meter.activeVoices);
    }
}

void AudioMixerTab::RenderContent()
{
    AudioMixer& mixer = GetMixer();
    if (ui::BeginTable("AudioMixerLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
    {
        ui::TableSetupColumn("Buses", ImGuiTableColumnFlags_WidthFixed, 420.0f);
        ui::TableSetupColumn("Inspector");
        ui::TableNextRow();
        ui::TableSetColumnIndex(0);
        RenderBuses(mixer);
        ui::TableSetColumnIndex(1);
        RenderBusInspector(mixer);
        RenderVoices(mixer);
        ui::EndTable();
    }
    RenderMeters(mixer);
    if (!validationError_.empty())
        ui::TextColored(ImVec4(1.0f, 0.35f, 0.25f, 1.0f), "Error: %s", validationError_.c_str());
}

void AudioMixerTab::RenderContextMenuItems()
{
    if (ui::MenuItem("Reset audio mixer template"))
        ResetDemoMixer();
}

void AudioMixerTab::OnResourceLoaded(const ea::string& resourceName)
{
    resource_ = GetSubsystem<ResourceCache>()->GetResource<AudioMixerResource>(resourceName);
    selectedBus_ = resource_ && resource_->GetMixer().GetBus("Master") ? "Master" : ea::string{};
    selectedVoice_.clear();
    status_ = resource_ ? "Audio mixer loaded" : "Unable to load audio mixer";
}

void AudioMixerTab::OnResourceUnloaded(const ea::string& resourceName)
{
    if (resourceName == GetActiveResourceName())
        resource_.Reset();
}

void AudioMixerTab::OnActiveResourceChanged(const ea::string&, const ea::string& newResourceName)
{
    if (newResourceName.empty())
    {
        resource_.Reset();
        return;
    }
    resource_ = GetSubsystem<ResourceCache>()->GetResource<AudioMixerResource>(newResourceName);
    selectedBus_ = resource_ && resource_->GetMixer().GetBus("Master") ? "Master" : ea::string{};
}

void AudioMixerTab::OnResourceSaved(const ea::string& resourceName)
{
    status_ = Format("Saved {}", resourceName);
}

void AudioMixerTab::OnResourceShallowSaved(const ea::string&)
{
}

} // namespace Urho3D
