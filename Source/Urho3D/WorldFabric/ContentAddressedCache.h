// Copyright (c) 2024-2026 robert-sarah and rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <EASTL/string.h>
#include <EASTL/unordered_map.h>

#include <Urho3D/Core/Attribute.h>

namespace Urho3D
{

struct URHO3D_API CachedArtifact
{
    ea::string digest;
    ea::string kind;
    ea::string location;
    unsigned long long size{};
    unsigned revision{};
};

/// Content-addressed artifact cache contract used by deterministic production builds.
class URHO3D_API ContentAddressedCache
{
public:
    bool Put(const CachedArtifact& artifact, ea::string* error = nullptr);
    bool Remove(const ea::string& digest, ea::string* error = nullptr);
    void Clear();

    bool Contains(const ea::string& digest) const;
    const CachedArtifact* Get(const ea::string& digest) const;
    unsigned GetSize() const { return artifacts_.size(); }

    bool Validate(ea::string* error = nullptr) const;
    unsigned long long ComputeDigest(ea::string* error = nullptr) const;

private:
    ea::unordered_map<ea::string, CachedArtifact> artifacts_;
};

}
