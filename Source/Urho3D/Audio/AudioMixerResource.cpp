// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#include "../Precompiled.h"

#include "AudioMixerResource.h"

#include "../IO/Deserializer.h"
#include "../IO/Serializer.h"
#include "../Resource/JSONFile.h"

namespace Urho3D
{

namespace
{

bool IsValidDspType(unsigned value)
{
    return value <= static_cast<unsigned>(AudioDspType::Delay);
}

bool IsValidBusItem(const JSONValue& item)
{
    return item.IsObject() && item.Get("name").IsString() && item.Get("parent").IsString()
        && item.Get("volume").IsNumber() && item.Get("muted").IsBool() && item.Get("soloed").IsBool()
        && item.Get("effects").IsArray();
}

} // namespace

AudioMixerResource::AudioMixerResource(Context* context)
    : Resource(context)
{
}

bool AudioMixerResource::CheckExtension(const ea::string& fileName)
{
    return fileName.ends_with(".audiomixer", false);
}

JSONValue AudioMixerResource::ToJSON() const
{
    JSONValue root(JSON_OBJECT);
    root.Set("version", 1u);

    JSONValue buses(JSON_ARRAY);
    for (const AudioBus& bus : mixer_.GetBuses())
    {
        JSONValue item(JSON_OBJECT);
        item.Set("name", bus.name);
        item.Set("parent", bus.parent);
        item.Set("volume", bus.volume);
        item.Set("muted", bus.muted);
        item.Set("soloed", bus.soloed);

        JSONValue effects(JSON_ARRAY);
        for (const AudioDspEffect& effect : bus.effects)
        {
            JSONValue effectItem(JSON_OBJECT);
            effectItem.Set("type", static_cast<unsigned>(effect.type));
            effectItem.Set("enabled", effect.enabled);
            effectItem.Set("mix", effect.mix);
            effectItem.Set("parameterA", effect.parameterA);
            effectItem.Set("parameterB", effect.parameterB);
            effects.Push(ea::move(effectItem));
        }
        item.Set("effects", ea::move(effects));
        buses.Push(ea::move(item));
    }
    root.Set("buses", ea::move(buses));

    JSONValue voices(JSON_ARRAY);
    for (const AudioVoice& voice : mixer_.GetVoices())
    {
        JSONValue item(JSON_OBJECT);
        item.Set("id", voice.id);
        item.Set("bus", voice.bus);
        item.Set("volume", voice.volume);
        item.Set("pan", voice.pan);
        item.Set("pitch", voice.pitch);
        item.Set("distanceAttenuation", voice.distanceAttenuation);
        item.Set("playing", voice.playing);
        voices.Push(ea::move(item));
    }
    root.Set("voices", ea::move(voices));
    return root;
}

bool AudioMixerResource::FromJSON(const JSONValue& root, ea::string* error)
{
    if (!root.IsObject() || !root.Get("buses").IsArray() || !root.Get("voices").IsArray())
    {
        if (error)
            *error = "AudioMixer requires buses and voices arrays";
        return false;
    }

    AudioMixer candidate;
    ea::vector<bool> added(root.Get("buses").GetArray().size(), false);
    unsigned remaining = added.size();
    while (remaining)
    {
        bool progress = false;
        for (unsigned index = 0; index < added.size(); ++index)
        {
            if (added[index])
                continue;
            const JSONValue& item = root.Get("buses").GetArray()[index];
            if (!IsValidBusItem(item))
            {
                if (error)
                    *error = "AudioMixer contains an invalid bus";
                return false;
            }

            const ea::string parent = item.Get("parent").GetString();
            if (!parent.empty() && !candidate.GetBus(parent))
                continue;

            AudioBus bus;
            bus.name = item.Get("name").GetString();
            bus.parent = parent;
            bus.volume = item.Get("volume").GetFloat();
            bus.muted = item.Get("muted").GetBool();
            bus.soloed = item.Get("soloed").GetBool();
            if (bus.name.empty() || !candidate.AddBus(bus))
            {
                if (error)
                    *error = Format("AudioMixer contains a duplicate or invalid bus: {}", bus.name);
                return false;
            }

            for (const JSONValue& effectItem : item.Get("effects").GetArray())
            {
                if (!effectItem.IsObject() || !effectItem.Get("type").IsNumber()
                    || !IsValidDspType(effectItem.Get("type").GetUInt()) || !effectItem.Get("enabled").IsBool()
                    || !effectItem.Get("mix").IsNumber() || !effectItem.Get("parameterA").IsNumber()
                    || !effectItem.Get("parameterB").IsNumber())
                {
                    if (error)
                        *error = Format("AudioMixer bus contains an invalid DSP effect: {}", bus.name);
                    return false;
                }
                AudioDspEffect effect;
                effect.type = static_cast<AudioDspType>(effectItem.Get("type").GetUInt());
                effect.enabled = effectItem.Get("enabled").GetBool();
                effect.mix = effectItem.Get("mix").GetFloat();
                effect.parameterA = effectItem.Get("parameterA").GetFloat();
                effect.parameterB = effectItem.Get("parameterB").GetFloat();
                if (!candidate.AddEffect(bus.name, effect))
                {
                    if (error)
                        *error = Format("AudioMixer could not add DSP effect to bus: {}", bus.name);
                    return false;
                }
            }

            added[index] = true;
            --remaining;
            progress = true;
        }

        if (!progress)
        {
            if (error)
                *error = "AudioMixer bus hierarchy contains a missing parent or cycle";
            return false;
        }
    }

    if (!candidate.GetBus("Master"))
    {
        if (error)
            *error = "AudioMixer requires a Master bus";
        return false;
    }

    for (const JSONValue& item : root.Get("voices").GetArray())
    {
        if (!item.IsObject() || !item.Get("id").IsString() || !item.Get("bus").IsString()
            || !item.Get("volume").IsNumber() || !item.Get("pan").IsNumber() || !item.Get("pitch").IsNumber()
            || !item.Get("distanceAttenuation").IsNumber() || !item.Get("playing").IsBool())
        {
            if (error)
                *error = "AudioMixer contains an invalid voice";
            return false;
        }
        AudioVoice voice;
        voice.id = item.Get("id").GetString();
        voice.bus = item.Get("bus").GetString();
        voice.volume = item.Get("volume").GetFloat();
        voice.pan = item.Get("pan").GetFloat();
        voice.pitch = item.Get("pitch").GetFloat();
        voice.distanceAttenuation = item.Get("distanceAttenuation").GetFloat();
        voice.playing = item.Get("playing").GetBool();
        if (!candidate.AddVoice(voice))
        {
            if (error)
                *error = Format("AudioMixer contains a duplicate or invalid voice: {}", voice.id);
            return false;
        }
    }

    mixer_ = ea::move(candidate);
    return true;
}

void AudioMixerResource::SerializeInBlock(Archive& archive)
{
    (void)archive;
}

bool AudioMixerResource::BeginLoad(Deserializer& source)
{
    JSONFile jsonFile(context_);
    if (!jsonFile.Load(source))
        return false;
    return FromJSON(jsonFile.GetRoot());
}

bool AudioMixerResource::Save(Serializer& dest) const
{
    JSONFile jsonFile(context_);
    jsonFile.GetRoot() = ToJSON();
    return jsonFile.Save(dest);
}

} // namespace Urho3D
