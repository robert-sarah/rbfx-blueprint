// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <Urho3D/Audio/AudioMixer.h>
#include <Urho3D/Resource/JSONValue.h>
#include <Urho3D/Resource/Resource.h>

namespace Urho3D
{

/// Loadable and editable production resource for an AudioMixer routing graph.
class URHO3D_API AudioMixerResource : public Resource
{
    URHO3D_OBJECT(AudioMixerResource, Resource);

public:
    explicit AudioMixerResource(Context* context);

    static bool CheckExtension(const ea::string& fileName);

    AudioMixer& GetMixer() { return mixer_; }
    const AudioMixer& GetMixer() const { return mixer_; }
    void SetMixer(const AudioMixer& mixer) { mixer_ = mixer; }

    JSONValue ToJSON() const;
    bool FromJSON(const JSONValue& root, ea::string* error = nullptr);

    void SerializeInBlock(Archive& archive) override;
    bool BeginLoad(Deserializer& source) override;
    bool Save(Serializer& dest) const override;

private:
    AudioMixer mixer_;
};

} // namespace Urho3D
