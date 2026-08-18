// Copyright (c) 2024-2026 robert-sarah and rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <EASTL/string.h>
#include <EASTL/unordered_map.h>

#include <Urho3D/Core/Attribute.h>
#include <Urho3D/Core/Variant.h>

namespace Urho3D
{

struct URHO3D_API HotReloadState
{
    ea::string objectId;
    unsigned generation{};
    StringVariantMap fields;
};

/// Runtime state bridge for preserving gameplay state across C++/Blueprint/rbscript reloads.
class URHO3D_API HotReloadStateStore
{
public:
    bool Capture(const ea::string& objectId, const StringVariantMap& fields, ea::string* error = nullptr);
    bool Restore(const ea::string& objectId, StringVariantMap& fields, ea::string* error = nullptr) const;
    bool Remove(const ea::string& objectId, ea::string* error = nullptr);
    void Clear();

    const HotReloadState* Get(const ea::string& objectId) const;
    unsigned GetGeneration(const ea::string& objectId) const;
    unsigned GetSize() const { return states_.size(); }

    bool Validate(ea::string* error = nullptr) const;
    unsigned long long ComputeDigest(ea::string* error = nullptr) const;

private:
    ea::unordered_map<ea::string, HotReloadState> states_;
};

}
