// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include "../Project/ResourceEditorTab.h"

#include <Urho3D/Audio/AudioMixerResource.h>

namespace Urho3D
{

void Foundation_AudioMixerTab(Context* context, Project* project);

/// Production audio mixer editor for hierarchical buses, DSP effects and voice routing.
class AudioMixerTab : public ResourceEditorTab
{
    URHO3D_OBJECT(AudioMixerTab, ResourceEditorTab);

public:
    explicit AudioMixerTab(Context* context);

    void RenderContent() override;
    void RenderToolbar() override;
    void RenderContextMenuItems() override;
    bool CanOpenResource(const ResourceFileDescriptor& desc) override;
    bool SupportMultipleResources() override { return false; }
    ea::string GetResourceTitle() override { return "Audio Mixer"; }
    bool IsUndoSupported() override { return true; }

    void ApplyMixerSnapshot(const JSONValue& snapshot);

protected:
    void OnResourceLoaded(const ea::string& resourceName) override;
    void OnResourceUnloaded(const ea::string& resourceName) override;
    void OnActiveResourceChanged(const ea::string& oldResourceName, const ea::string& newResourceName) override;
    void OnResourceSaved(const ea::string& resourceName) override;
    void OnResourceShallowSaved(const ea::string& resourceName) override;

private:
    AudioMixer& GetMixer();
    const AudioMixer& GetMixer() const;
    JSONValue CaptureMixer() const;
    void CommitMixerEdit(const JSONValue& before, const ea::string& status);
    void ResetDemoMixer();
    void RenderBuses(AudioMixer& mixer);
    void RenderBusInspector(AudioMixer& mixer);
    void RenderVoices(const AudioMixer& mixer);
    void RenderMeters(const AudioMixer& mixer);

    SharedPtr<AudioMixerResource> resource_;
    AudioMixer previewMixer_;
    ea::string status_;
    ea::string validationError_;
    ea::string selectedBus_;
    ea::string selectedVoice_;
    unsigned newEffectType_{};
};

} // namespace Urho3D
