// Copyright (c) 2024-2026 robert-sarah and rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#include "ContentAddressedCache.h"

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

bool ContentAddressedCache::Put(const CachedArtifact& artifact, ea::string* error)
{
    if (artifact.digest.empty())
    {
        SetError(error, "Cached artifact digest must not be empty");
        return false;
    }
    if (artifact.kind.empty())
    {
        SetError(error, "Cached artifact kind must not be empty");
        return false;
    }
    if (artifact.location.empty())
    {
        SetError(error, "Cached artifact location must not be empty");
        return false;
    }
    if (artifact.digest.find("sha256:") != 0 && artifact.digest.find("fnv1a:") != 0)
    {
        SetError(error, "Cached artifact digest must use sha256: or fnv1a: prefix");
        return false;
    }

    CachedArtifact stored = artifact;
    const auto existing = artifacts_.find(artifact.digest);
    if (existing != artifacts_.end())
        stored.revision = existing->second.revision + 1;
    else if (stored.revision == 0)
        stored.revision = 1;
    artifacts_[stored.digest] = ea::move(stored);
    return true;
}

bool ContentAddressedCache::Remove(const ea::string& digest, ea::string* error)
{
    const auto it = artifacts_.find(digest);
    if (it == artifacts_.end())
    {
        SetError(error, "Cached artifact was not found: " + digest);
        return false;
    }
    artifacts_.erase(it);
    return true;
}

void ContentAddressedCache::Clear()
{
    artifacts_.clear();
}

bool ContentAddressedCache::Contains(const ea::string& digest) const
{
    return artifacts_.find(digest) != artifacts_.end();
}

const CachedArtifact* ContentAddressedCache::Get(const ea::string& digest) const
{
    const auto it = artifacts_.find(digest);
    return it != artifacts_.end() ? &it->second : nullptr;
}

bool ContentAddressedCache::Validate(ea::string* error) const
{
    for (const auto& pair : artifacts_)
    {
        if (pair.first.empty() || pair.second.digest != pair.first)
        {
            SetError(error, "Cached artifact key and digest must match");
            return false;
        }
        if (pair.second.kind.empty() || pair.second.location.empty())
        {
            SetError(error, "Cached artifact metadata is incomplete: " + pair.first);
            return false;
        }
    }
    return true;
}

unsigned long long ContentAddressedCache::ComputeDigest(ea::string* error) const
{
    if (!Validate(error))
        return 0;

    ea::vector<ea::string> digests;
    digests.reserve(artifacts_.size());
    for (const auto& pair : artifacts_)
        digests.push_back(pair.first);
    std::sort(digests.begin(), digests.end());

    ea::string serialized = "content-cache-v1\\n";
    for (const ea::string& digest : digests)
    {
        const CachedArtifact& artifact = artifacts_.at(digest);
        serialized += artifact.digest + "|" + artifact.kind + "|" + artifact.location
            + "|" + ToString("%llu", artifact.size) + "|" + ToString("%u", artifact.revision) + "\\n";
    }
    return HashText(serialized);
}

}
