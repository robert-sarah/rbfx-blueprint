// Copyright (c) 2026 the rbfx-blueprint project.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <Urho3D/Urho3D.h>
#include <Urho3D/Resource/JSONValue.h>

#include <string>
#include <vector>

namespace Urho3D
{

struct URHO3D_API SemanticCapsuleEntry
{
    std::string path;
    std::string category;
    std::string platform;
    unsigned long long size{};
    unsigned long long contentDigest{};
};

struct URHO3D_API SemanticCapsulePlugin
{
    std::string id;
    std::string version;
    unsigned long long digest{};
};

struct URHO3D_API SemanticBuildCapsuleMetadata
{
    std::string engineRevision;
    std::string toolchain;
    std::string platform;
    std::string architecture;
    std::string configuration;
    unsigned long long worldFabricDigest{};
    unsigned long long timeMachineDigest{};
};

struct URHO3D_API SemanticCapsuleDiff
{
    bool valid{};
    bool metadataChanged{};
    std::vector<std::string> addedEntries;
    std::vector<std::string> removedEntries;
    std::vector<std::string> changedEntries;
    std::vector<std::string> addedPlugins;
    std::vector<std::string> removedPlugins;
    std::vector<std::string> changedPlugins;
};

/// Immutable-style, deterministic evidence package for builds, bugs and releases.
class URHO3D_API SemanticBuildCapsule
{
public:
    SemanticBuildCapsule() = default;

    const SemanticBuildCapsuleMetadata& GetMetadata() const { return metadata_; }
    const std::vector<SemanticCapsuleEntry>& GetEntries() const { return entries_; }
    const std::vector<SemanticCapsulePlugin>& GetPlugins() const { return plugins_; }

    void SetMetadata(const SemanticBuildCapsuleMetadata& metadata) { metadata_ = metadata; }
    void Clear();
    bool AddEntry(const SemanticCapsuleEntry& entry, std::string* error = nullptr);
    bool AddPlugin(const SemanticCapsulePlugin& plugin, std::string* error = nullptr);

    bool Validate(std::string* error = nullptr) const;
    JSONValue ToJSON() const;
    bool FromJSON(const JSONValue& root, std::string* error = nullptr);
    std::string ToCanonicalText() const;
    unsigned long long ComputeDigest() const;
    SemanticCapsuleDiff Diff(const SemanticBuildCapsule& other) const;

private:
    static bool IsValidDigest(unsigned long long digest) { return digest != 0; }
    static std::string DigestText(unsigned long long digest);
    static bool ParseDigest(const JSONValue& value, unsigned long long& digest);
    static void SetError(std::string* error, const std::string& message);
    static bool EntryLess(const SemanticCapsuleEntry& left, const SemanticCapsuleEntry& right);
    static bool PluginLess(const SemanticCapsulePlugin& left, const SemanticCapsulePlugin& right);

    SemanticBuildCapsuleMetadata metadata_;
    std::vector<SemanticCapsuleEntry> entries_;
    std::vector<SemanticCapsulePlugin> plugins_;
};

} // namespace Urho3D
