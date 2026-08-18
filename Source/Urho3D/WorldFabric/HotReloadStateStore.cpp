// Copyright (c) 2024-2026 robert-sarah and rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#include "HotReloadStateStore.h"

#include <Urho3D/Core/StringUtils.h>

#include <algorithm>

namespace Urho3D
{
namespace
{

void SetError(ea::string* error, const ea::string& message)
{
    if (error)
        *error = message;
}

unsigned long long HashText(const ea::string& text)
{
    unsigned long long digest = 1469598103934665603ull;
    for (const unsigned char character : text)
    {
        digest ^= character;
        digest *= 1099511628211ull;
    }
    return digest;
}

}

bool HotReloadStateStore::Capture(const ea::string& objectId, const StringVariantMap& fields, ea::string* error)
{
    if (objectId.empty())
    {
        SetError(error, "Hot reload object id must not be empty");
        return false;
    }

    HotReloadState state;
    state.objectId = objectId;
    const auto existing = states_.find(objectId);
    state.generation = existing != states_.end() ? existing->second.generation + 1 : 1;
    state.fields = fields;
    states_[objectId] = ea::move(state);
    return true;
}

bool HotReloadStateStore::Restore(const ea::string& objectId, StringVariantMap& fields, ea::string* error) const
{
    const auto it = states_.find(objectId);
    if (it == states_.end())
    {
        SetError(error, "Hot reload state was not found: " + objectId);
        return false;
    }
    fields = it->second.fields;
    return true;
}

bool HotReloadStateStore::Remove(const ea::string& objectId, ea::string* error)
{
    const auto it = states_.find(objectId);
    if (it == states_.end())
    {
        SetError(error, "Hot reload state was not found: " + objectId);
        return false;
    }
    states_.erase(it);
    return true;
}

void HotReloadStateStore::Clear()
{
    states_.clear();
}

const HotReloadState* HotReloadStateStore::Get(const ea::string& objectId) const
{
    const auto it = states_.find(objectId);
    return it != states_.end() ? &it->second : nullptr;
}

unsigned HotReloadStateStore::GetGeneration(const ea::string& objectId) const
{
    const HotReloadState* state = Get(objectId);
    return state ? state->generation : 0;
}

bool HotReloadStateStore::Validate(ea::string* error) const
{
    for (const auto& pair : states_)
    {
        if (pair.first.empty() || pair.second.objectId != pair.first || pair.second.generation == 0)
        {
            SetError(error, "Hot reload state has an invalid identity or generation");
            return false;
        }
    }
    return true;
}

unsigned long long HotReloadStateStore::ComputeDigest(ea::string* error) const
{
    if (!Validate(error))
        return 0;

    ea::vector<ea::string> objectIds;
    objectIds.reserve(states_.size());
    for (const auto& pair : states_)
        objectIds.push_back(pair.first);
    std::sort(objectIds.begin(), objectIds.end());

    ea::string serialized = "hot-reload-state-v1\\n";
    for (const ea::string& objectId : objectIds)
    {
        const HotReloadState& state = states_.find(objectId)->second;
        serialized += state.objectId + "|" + ToString("%u", state.generation) + "|";

        ea::vector<ea::string> fieldNames;
        fieldNames.reserve(state.fields.size());
        for (const auto& field : state.fields)
            fieldNames.push_back(field.first);
        std::sort(fieldNames.begin(), fieldNames.end());
        for (const ea::string& fieldName : fieldNames)
        {
            const auto fieldIt = state.fields.find(fieldName);
            serialized += fieldName + ":" + ToString("%u", fieldIt->second.GetType()) + ":"
                + fieldIt->second.ToString() + ";";
        }
        serialized += "\\n";
    }
    return HashText(serialized);
}

}
