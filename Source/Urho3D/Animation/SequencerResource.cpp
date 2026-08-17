// SPDX-License-Identifier: MIT

#include "../Precompiled.h"

#include "SequencerResource.h"

#include "../IO/Deserializer.h"
#include "../IO/Serializer.h"
#include "../Resource/JSONFile.h"

#include <cmath>

namespace Urho3D
{

namespace
{

JSONValue MakeVariantJSON(const Variant& value, Context* context)
{
    JSONValue json;
    json.SetVariant(value, context);
    return json;
}

bool IsValidTrackType(unsigned value)
{
    return value <= static_cast<unsigned>(SequencerTrackType::Event);
}

} // namespace

SequencerResource::SequencerResource(Context* context)
    : Resource(context)
{
}

bool SequencerResource::CheckExtension(const ea::string& fileName)
{
    return fileName.ends_with(".sequence", false) || fileName.ends_with(".sequencer", false);
}

JSONValue SequencerResource::ToJSON() const
{
    JSONValue root(JSON_OBJECT);
    root.Set("version", 1u);
    root.Set("duration", sequencer_.GetDuration());
    root.Set("looping", sequencer_.IsLooping());

    JSONValue tracks(JSON_ARRAY);
    for (const SequencerTrack& track : sequencer_.GetTracks())
    {
        JSONValue trackJson(JSON_OBJECT);
        trackJson.Set("name", track.name);
        trackJson.Set("type", static_cast<unsigned>(track.type));
        trackJson.Set("muted", track.muted);

        JSONValue keyframes(JSON_ARRAY);
        for (const SequencerKeyframe& keyframe : track.keyframes)
        {
            JSONValue keyframeJson(JSON_OBJECT);
            keyframeJson.Set("time", keyframe.time);
            keyframeJson.Set("value", MakeVariantJSON(keyframe.value, context_));
            keyframes.Push(ea::move(keyframeJson));
        }
        trackJson.Set("keyframes", ea::move(keyframes));
        tracks.Push(ea::move(trackJson));
    }
    root.Set("tracks", ea::move(tracks));
    return root;
}

bool SequencerResource::FromJSON(const JSONValue& root, ea::string* error)
{
    if (!root.IsObject())
    {
        if (error)
            *error = "Sequencer root must be a JSON object";
        return false;
    }

    const JSONValue& version = root.Get("version");
    const JSONValue& duration = root.Get("duration");
    const JSONValue& looping = root.Get("looping");
    const JSONValue& tracks = root.Get("tracks");
    if (!version.IsNumber() || version.GetUInt() != 1u || !duration.IsNumber() || !looping.IsBool() || !tracks.IsArray())
    {
        if (error)
            *error = "Sequencer requires version 1, duration, looping and tracks";
        return false;
    }

    const float durationValue = duration.GetFloat();
    if (!(durationValue > 0.0f) || !std::isfinite(durationValue))
    {
        if (error)
            *error = "Sequencer duration must be a finite positive number";
        return false;
    }

    Sequencer candidate;
    candidate.SetDuration(durationValue);
    candidate.SetLooping(looping.GetBool());

    for (const JSONValue& trackJson : tracks.GetArray())
    {
        if (!trackJson.IsObject() || !trackJson.Get("name").IsString() || !trackJson.Get("type").IsNumber()
            || !trackJson.Get("muted").IsBool() || !trackJson.Get("keyframes").IsArray()
            || !IsValidTrackType(trackJson.Get("type").GetUInt()))
        {
            if (error)
                *error = "Sequencer contains an invalid track";
            return false;
        }

        ea::string trackError;
        const ea::string trackName = trackJson.Get("name").GetString();
        if (!candidate.AddTrack(trackName, static_cast<SequencerTrackType>(trackJson.Get("type").GetUInt()), &trackError))
        {
            if (error)
                *error = Format("Invalid Sequencer track '{}': {}", trackName, trackError);
            return false;
        }

        SequencerTrack* track = candidate.GetTrack(trackName);
        if (!track)
        {
            if (error)
                *error = Format("Sequencer track '{}' could not be created", trackName);
            return false;
        }
        track->muted = trackJson.Get("muted").GetBool();

        for (const JSONValue& keyframeJson : trackJson.Get("keyframes").GetArray())
        {
            if (!keyframeJson.IsObject() || !keyframeJson.Get("time").IsNumber())
            {
                if (error)
                    *error = Format("Sequencer track '{}' contains an invalid keyframe", trackName);
                return false;
            }

            SequencerKeyframe keyframe;
            keyframe.time = keyframeJson.Get("time").GetFloat();
            keyframe.value = keyframeJson.Get("value").GetVariant();
            ea::string keyframeError;
            if (!candidate.AddKeyframe(trackName, keyframe, &keyframeError))
            {
                if (error)
                    *error = Format("Invalid keyframe in track '{}': {}", trackName, keyframeError);
                return false;
            }
        }
    }

    sequencer_ = ea::move(candidate);
    return true;
}

void SequencerResource::SerializeInBlock(Archive& archive)
{
    (void)archive;
}

bool SequencerResource::BeginLoad(Deserializer& source)
{
    JSONFile jsonFile(context_);
    if (!jsonFile.Load(source))
        return false;
    return FromJSON(jsonFile.GetRoot());
}

bool SequencerResource::Save(Serializer& dest) const
{
    JSONFile jsonFile(context_);
    jsonFile.GetRoot() = ToJSON();
    return jsonFile.Save(dest);
}

} // namespace Urho3D
