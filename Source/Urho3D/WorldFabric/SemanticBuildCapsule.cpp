// Copyright (c) 2026 the rbfx-blueprint project.
//
// SPDX-License-Identifier: MIT
//

#include "SemanticBuildCapsule.h"

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <sstream>
#include <unordered_map>

namespace Urho3D
{

namespace
{

constexpr std::uint64_t FnvOffset = 1469598103934665603ull;
constexpr std::uint64_t FnvPrime = 1099511628211ull;

void HashByte(std::uint64_t& hash, unsigned char value)
{
    hash ^= value;
    hash *= FnvPrime;
}

void HashText(std::uint64_t& hash, const std::string& value)
{
    HashByte(hash, static_cast<unsigned char>(value.size() & 0xff));
    for (const unsigned char byte : value)
        HashByte(hash, byte);
    HashByte(hash, 0);
}

void HashValue(std::uint64_t& hash, std::uint64_t value)
{
    for (unsigned i = 0; i < sizeof(value); ++i)
        HashByte(hash, static_cast<unsigned char>((value >> (i * 8)) & 0xff));
}

ea::string Text(const std::string& value)
{
    return ea::string(value.c_str());
}

JSONValue DigestValue(unsigned long long digest)
{
    return JSONValue(Text(std::to_string(digest)));
}

bool ReadString(const JSONValue& object, const char* key, std::string& target)
{
    const JSONValue& value = object.Get(key);
    if (!value.IsString() || value.GetString().empty())
        return false;
    target = value.GetString().c_str();
    return true;
}

} // namespace

void SemanticBuildCapsule::Clear()
{
    metadata_ = {};
    entries_.clear();
    plugins_.clear();
}

bool SemanticBuildCapsule::AddEntry(const SemanticCapsuleEntry& entry, std::string* error)
{
    if (entry.path.empty() || entry.category.empty() || entry.platform.empty())
    {
        SetError(error, "A capsule entry requires a path, category and platform.");
        return false;
    }
    if (!IsValidDigest(entry.contentDigest))
    {
        SetError(error, "A capsule entry requires a non-zero content digest.");
        return false;
    }
    if (std::any_of(entries_.begin(), entries_.end(), [&entry](const SemanticCapsuleEntry& existing) {
        return existing.path == entry.path;
    }))
    {
        SetError(error, "The capsule already contains an entry with this path.");
        return false;
    }
    entries_.push_back(entry);
    return true;
}

bool SemanticBuildCapsule::AddPlugin(const SemanticCapsulePlugin& plugin, std::string* error)
{
    if (plugin.id.empty() || plugin.version.empty())
    {
        SetError(error, "A capsule plugin requires an id and version.");
        return false;
    }
    if (!IsValidDigest(plugin.digest))
    {
        SetError(error, "A capsule plugin requires a non-zero digest.");
        return false;
    }
    if (std::any_of(plugins_.begin(), plugins_.end(), [&plugin](const SemanticCapsulePlugin& existing) {
        return existing.id == plugin.id;
    }))
    {
        SetError(error, "The capsule already contains a plugin with this id.");
        return false;
    }
    plugins_.push_back(plugin);
    return true;
}

bool SemanticBuildCapsule::Validate(std::string* error) const
{
    if (metadata_.engineRevision.empty() || metadata_.toolchain.empty() || metadata_.platform.empty()
        || metadata_.architecture.empty() || metadata_.configuration.empty())
    {
        SetError(error, "Capsule metadata requires engine revision, toolchain, platform, architecture and configuration.");
        return false;
    }
    if (!IsValidDigest(metadata_.worldFabricDigest))
    {
        SetError(error, "Capsule metadata requires a non-zero World Fabric digest.");
        return false;
    }
    for (size_t i = 0; i < entries_.size(); ++i)
    {
        std::string entryError;
        SemanticBuildCapsule candidate;
        candidate.metadata_ = metadata_;
        for (size_t j = 0; j < i; ++j)
            candidate.entries_.push_back(entries_[j]);
        if (!candidate.AddEntry(entries_[i], &entryError))
        {
            SetError(error, entryError);
            return false;
        }
    }
    for (size_t i = 0; i < plugins_.size(); ++i)
    {
        if (plugins_[i].id.empty() || plugins_[i].version.empty() || !IsValidDigest(plugins_[i].digest))
        {
            SetError(error, "Capsule contains an invalid plugin entry.");
            return false;
        }
        for (size_t j = 0; j < i; ++j)
        {
            if (plugins_[j].id == plugins_[i].id)
            {
                SetError(error, "Capsule contains duplicate plugin ids.");
                return false;
            }
        }
    }
    return true;
}

JSONValue SemanticBuildCapsule::ToJSON() const
{
    JSONValue root(JSON_OBJECT);
    root.Set("version", 1u);

    JSONValue metadata(JSON_OBJECT);
    metadata.Set("engineRevision", Text(metadata_.engineRevision));
    metadata.Set("toolchain", Text(metadata_.toolchain));
    metadata.Set("platform", Text(metadata_.platform));
    metadata.Set("architecture", Text(metadata_.architecture));
    metadata.Set("configuration", Text(metadata_.configuration));
    metadata.Set("worldFabricDigest", DigestValue(metadata_.worldFabricDigest));
    metadata.Set("timeMachineDigest", DigestValue(metadata_.timeMachineDigest));
    root.Set("metadata", ea::move(metadata));

    std::vector<SemanticCapsuleEntry> entries = entries_;
    std::sort(entries.begin(), entries.end(), EntryLess);
    JSONValue entryArray(JSON_ARRAY);
    for (const SemanticCapsuleEntry& entry : entries)
    {
        JSONValue value(JSON_OBJECT);
        value.Set("path", Text(entry.path));
        value.Set("category", Text(entry.category));
        value.Set("platform", Text(entry.platform));
        value.Set("size", DigestValue(entry.size));
        value.Set("contentDigest", DigestValue(entry.contentDigest));
        entryArray.Push(ea::move(value));
    }
    root.Set("entries", ea::move(entryArray));

    std::vector<SemanticCapsulePlugin> plugins = plugins_;
    std::sort(plugins.begin(), plugins.end(), PluginLess);
    JSONValue pluginArray(JSON_ARRAY);
    for (const SemanticCapsulePlugin& plugin : plugins)
    {
        JSONValue value(JSON_OBJECT);
        value.Set("id", Text(plugin.id));
        value.Set("version", Text(plugin.version));
        value.Set("digest", DigestValue(plugin.digest));
        pluginArray.Push(ea::move(value));
    }
    root.Set("plugins", ea::move(pluginArray));
    return root;
}

bool SemanticBuildCapsule::FromJSON(const JSONValue& root, std::string* error)
{
    if (!root.IsObject() || !root.Get("version").IsNumber() || root.Get("version").GetUInt() != 1u
        || !root.Get("metadata").IsObject() || !root.Get("entries").IsArray() || !root.Get("plugins").IsArray())
    {
        SetError(error, "Semantic Build Capsule requires version 1, metadata, entries and plugins.");
        return false;
    }

    const JSONValue& metadata = root.Get("metadata");
    SemanticBuildCapsule candidate;
    if (!ReadString(metadata, "engineRevision", candidate.metadata_.engineRevision)
        || !ReadString(metadata, "toolchain", candidate.metadata_.toolchain)
        || !ReadString(metadata, "platform", candidate.metadata_.platform)
        || !ReadString(metadata, "architecture", candidate.metadata_.architecture)
        || !ReadString(metadata, "configuration", candidate.metadata_.configuration)
        || !ParseDigest(metadata.Get("worldFabricDigest"), candidate.metadata_.worldFabricDigest)
        || !ParseDigest(metadata.Get("timeMachineDigest"), candidate.metadata_.timeMachineDigest))
    {
        SetError(error, "Semantic Build Capsule metadata is incomplete or invalid.");
        return false;
    }

    for (const JSONValue& value : root.Get("entries").GetArray())
    {
        if (!value.IsObject())
        {
            SetError(error, "Semantic Build Capsule contains an invalid entry.");
            return false;
        }
        SemanticCapsuleEntry entry;
        if (!ReadString(value, "path", entry.path) || !ReadString(value, "category", entry.category)
            || !ReadString(value, "platform", entry.platform) || !ParseDigest(value.Get("size"), entry.size)
            || !ParseDigest(value.Get("contentDigest"), entry.contentDigest) || !candidate.AddEntry(entry, error))
            return false;
    }

    for (const JSONValue& value : root.Get("plugins").GetArray())
    {
        if (!value.IsObject())
        {
            SetError(error, "Semantic Build Capsule contains an invalid plugin.");
            return false;
        }
        SemanticCapsulePlugin plugin;
        if (!ReadString(value, "id", plugin.id) || !ReadString(value, "version", plugin.version)
            || !ParseDigest(value.Get("digest"), plugin.digest) || !candidate.AddPlugin(plugin, error))
            return false;
    }

    if (!candidate.Validate(error))
        return false;
    *this = std::move(candidate);
    return true;
}

std::string SemanticBuildCapsule::ToCanonicalText() const
{
    std::ostringstream stream;
    stream << "version=1\n";
    stream << "engineRevision=" << metadata_.engineRevision << '\n';
    stream << "toolchain=" << metadata_.toolchain << '\n';
    stream << "platform=" << metadata_.platform << '\n';
    stream << "architecture=" << metadata_.architecture << '\n';
    stream << "configuration=" << metadata_.configuration << '\n';
    stream << "worldFabricDigest=" << metadata_.worldFabricDigest << '\n';
    stream << "timeMachineDigest=" << metadata_.timeMachineDigest << '\n';

    std::vector<SemanticCapsuleEntry> entries = entries_;
    std::sort(entries.begin(), entries.end(), EntryLess);
    for (const SemanticCapsuleEntry& entry : entries)
        stream << "entry|" << entry.path << '|' << entry.category << '|' << entry.platform << '|'
               << entry.size << '|' << entry.contentDigest << '\n';

    std::vector<SemanticCapsulePlugin> plugins = plugins_;
    std::sort(plugins.begin(), plugins.end(), PluginLess);
    for (const SemanticCapsulePlugin& plugin : plugins)
        stream << "plugin|" << plugin.id << '|' << plugin.version << '|' << plugin.digest << '\n';
    return stream.str();
}

unsigned long long SemanticBuildCapsule::ComputeDigest() const
{
    std::uint64_t hash = FnvOffset;
    const std::string canonical = ToCanonicalText();
    for (const unsigned char byte : canonical)
        HashByte(hash, byte);
    return hash;
}

SemanticCapsuleDiff SemanticBuildCapsule::Diff(const SemanticBuildCapsule& other) const
{
    SemanticCapsuleDiff diff;
    diff.valid = Validate() && other.Validate();
    if (!diff.valid)
        return diff;

    diff.metadataChanged = metadata_.engineRevision != other.metadata_.engineRevision
        || metadata_.toolchain != other.metadata_.toolchain || metadata_.platform != other.metadata_.platform
        || metadata_.architecture != other.metadata_.architecture || metadata_.configuration != other.metadata_.configuration
        || metadata_.worldFabricDigest != other.metadata_.worldFabricDigest
        || metadata_.timeMachineDigest != other.metadata_.timeMachineDigest;

    std::unordered_map<std::string, const SemanticCapsuleEntry*> leftEntries;
    std::unordered_map<std::string, const SemanticCapsuleEntry*> rightEntries;
    for (const auto& entry : entries_) leftEntries[entry.path] = &entry;
    for (const auto& entry : other.entries_) rightEntries[entry.path] = &entry;
    for (const auto& pair : leftEntries)
    {
        const auto right = rightEntries.find(pair.first);
        if (right == rightEntries.end()) diff.removedEntries.push_back(pair.first);
        else if (pair.second->category != right->second->category || pair.second->platform != right->second->platform
            || pair.second->size != right->second->size || pair.second->contentDigest != right->second->contentDigest)
            diff.changedEntries.push_back(pair.first);
    }
    for (const auto& pair : rightEntries)
        if (leftEntries.find(pair.first) == leftEntries.end()) diff.addedEntries.push_back(pair.first);

    std::unordered_map<std::string, const SemanticCapsulePlugin*> leftPlugins;
    std::unordered_map<std::string, const SemanticCapsulePlugin*> rightPlugins;
    for (const auto& plugin : plugins_) leftPlugins[plugin.id] = &plugin;
    for (const auto& plugin : other.plugins_) rightPlugins[plugin.id] = &plugin;
    for (const auto& pair : leftPlugins)
    {
        const auto right = rightPlugins.find(pair.first);
        if (right == rightPlugins.end()) diff.removedPlugins.push_back(pair.first);
        else if (pair.second->version != right->second->version || pair.second->digest != right->second->digest)
            diff.changedPlugins.push_back(pair.first);
    }
    for (const auto& pair : rightPlugins)
        if (leftPlugins.find(pair.first) == leftPlugins.end()) diff.addedPlugins.push_back(pair.first);

    std::sort(diff.addedEntries.begin(), diff.addedEntries.end());
    std::sort(diff.removedEntries.begin(), diff.removedEntries.end());
    std::sort(diff.changedEntries.begin(), diff.changedEntries.end());
    std::sort(diff.addedPlugins.begin(), diff.addedPlugins.end());
    std::sort(diff.removedPlugins.begin(), diff.removedPlugins.end());
    std::sort(diff.changedPlugins.begin(), diff.changedPlugins.end());
    return diff;
}

std::string SemanticBuildCapsule::DigestText(unsigned long long digest)
{
    return std::to_string(digest);
}

bool SemanticBuildCapsule::ParseDigest(const JSONValue& value, unsigned long long& digest)
{
    if (!value.IsString() || value.GetString().empty())
        return false;
    errno = 0;
    char* end = nullptr;
    const unsigned long long parsed = std::strtoull(value.GetCString(), &end, 10);
    if (errno != 0 || !end || *end != '\0')
        return false;
    digest = parsed;
    return true;
}

void SemanticBuildCapsule::SetError(std::string* error, const std::string& message)
{
    if (error)
        *error = message;
}

bool SemanticBuildCapsule::EntryLess(const SemanticCapsuleEntry& left, const SemanticCapsuleEntry& right)
{
    return left.path < right.path;
}

bool SemanticBuildCapsule::PluginLess(const SemanticCapsulePlugin& left, const SemanticCapsulePlugin& right)
{
    return left.id < right.id;
}

} // namespace Urho3D
