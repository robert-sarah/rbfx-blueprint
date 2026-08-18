// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#include "DistributedPackageRegistry.h"

#include "JSONFile.h"
#include "../Core/StringUtils.h"
#include "../IO/Deserializer.h"
#include "../IO/Serializer.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace Urho3D
{

namespace
{

bool ParseUnsigned(const ea::string& text, unsigned long long& value)
{
    if (text.empty())
        return false;
    char* end = nullptr;
    const unsigned long long parsed = std::strtoull(text.c_str(), &end, 10);
    if (!end || *end != '\0')
        return false;
    value = parsed;
    return true;
}

bool ParseVersionParts(const ea::string& text, unsigned& major, unsigned& minor, unsigned& patch)
{
    const ea::vector<ea::string> parts = text.split('.');
    return parts.size() == 3 && !parts[0].empty() && !parts[1].empty() && !parts[2].empty()
        && std::all_of(parts[0].begin(), parts[0].end(), [](char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; })
        && std::all_of(parts[1].begin(), parts[1].end(), [](char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; })
        && std::all_of(parts[2].begin(), parts[2].end(), [](char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; })
        && (major = static_cast<unsigned>(std::strtoul(parts[0].c_str(), nullptr, 10)),
            minor = static_cast<unsigned>(std::strtoul(parts[1].c_str(), nullptr, 10)),
            patch = static_cast<unsigned>(std::strtoul(parts[2].c_str(), nullptr, 10)), true);
}

} // namespace

DistributedPackageRegistry::DistributedPackageRegistry(Context* context)
    : Resource(context)
{
}

bool DistributedPackageRegistry::CheckExtension(const ea::string& fileName)
{
    return fileName.ends_with(".packageregistry", false) || fileName.ends_with(".packages", false)
        || fileName.ends_with(".packageindex", false);
}

void DistributedPackageRegistry::Reset()
{
    packages_.clear();
    repositories_.clear();
    lastError_.clear();
}

void DistributedPackageRegistry::SetError(ea::string* error, const ea::string& message) const
{
    lastError_ = message;
    if (error)
        *error = message;
}

bool DistributedPackageRegistry::IsValidVersion(const ea::string& version)
{
    unsigned major{}, minor{}, patch{};
    return ParseVersionParts(version, major, minor, patch);
}

int DistributedPackageRegistry::CompareVersions(const ea::string& lhs, const ea::string& rhs)
{
    unsigned lhsMajor{}, lhsMinor{}, lhsPatch{};
    unsigned rhsMajor{}, rhsMinor{}, rhsPatch{};
    if (!ParseVersionParts(lhs, lhsMajor, lhsMinor, lhsPatch) || !ParseVersionParts(rhs, rhsMajor, rhsMinor, rhsPatch))
        return lhs < rhs ? -1 : (lhs == rhs ? 0 : 1);
    if (lhsMajor != rhsMajor)
        return lhsMajor < rhsMajor ? -1 : 1;
    if (lhsMinor != rhsMinor)
        return lhsMinor < rhsMinor ? -1 : 1;
    if (lhsPatch != rhsPatch)
        return lhsPatch < rhsPatch ? -1 : 1;
    return 0;
}

bool DistributedPackageRegistry::MatchesVersion(const ea::string& version, const ea::string& range)
{
    if (range.empty() || range == "*")
        return true;
    if (range[0] == '^' || range[0] == '~')
    {
        const ea::string base = range.substr(1);
        unsigned major{}, minor{}, patch{};
        unsigned versionMajor{}, versionMinor{}, versionPatch{};
        if (!ParseVersionParts(base, major, minor, patch) || !ParseVersionParts(version, versionMajor, versionMinor, versionPatch))
            return false;
        if (CompareVersions(version, base) < 0)
            return false;
        return range[0] == '^' ? versionMajor == major : (versionMajor == major && versionMinor == minor);
    }
    if (range.starts_with(">="))
        return CompareVersions(version, range.substr(2)) >= 0;
    if (range.starts_with(">"))
        return CompareVersions(version, range.substr(1)) > 0;
    if (range.starts_with("<="))
        return CompareVersions(version, range.substr(2)) <= 0;
    if (range.starts_with("<"))
        return CompareVersions(version, range.substr(1)) < 0;
    return version == range;
}

bool DistributedPackageRegistry::AddRepository(const PackageRepositoryEndpoint& repository, ea::string* error)
{
    if (repository.id.empty() || repository.url.empty())
    {
        SetError(error, "Package repository identifiers and URLs must not be empty.");
        return false;
    }
    if (repository.url.find('\n') != ea::string::npos || repository.url.find('\r') != ea::string::npos)
    {
        SetError(error, "Package repository URLs must not contain line breaks.");
        return false;
    }
    for (const PackageRepositoryEndpoint& existing : repositories_)
    {
        if (existing.id == repository.id)
        {
            SetError(error, Format("Package repository '{}' is already registered.", repository.id));
            return false;
        }
    }
    repositories_.push_back(repository);
    std::sort(repositories_.begin(), repositories_.end(), [](const auto& lhs, const auto& rhs)
    {
        if (lhs.priority != rhs.priority)
            return lhs.priority > rhs.priority;
        return lhs.id < rhs.id;
    });
    return true;
}

bool DistributedPackageRegistry::RemoveRepository(const ea::string& repositoryId, ea::string* error)
{
    auto it = std::find_if(repositories_.begin(), repositories_.end(), [&repositoryId](const auto& repository)
    {
        return repository.id == repositoryId;
    });
    if (it == repositories_.end())
    {
        SetError(error, Format("Package repository '{}' is not registered.", repositoryId));
        return false;
    }
    repositories_.erase(it);
    return true;
}

bool DistributedPackageRegistry::ValidatePackage(const DistributedPackage& package, ea::string* error) const
{
    if (package.name.empty() || package.version.empty() || package.architecture.empty() || package.digest.empty() || package.artifact.empty())
    {
        SetError(error, "Distributed package name, version, architecture, digest and artifact are required.");
        return false;
    }
    if (!IsValidVersion(package.version))
    {
        SetError(error, Format("Distributed package '{}' has an invalid semantic version '{}'.", package.name, package.version));
        return false;
    }
    if (package.digest.find_first_of(" \t\r\n") != ea::string::npos)
    {
        SetError(error, Format("Distributed package '{}' has an invalid content digest.", package.name));
        return false;
    }
    for (const ea::string& dependency : package.dependencies)
    {
        if (dependency.empty())
        {
            SetError(error, Format("Distributed package '{}' contains an empty dependency.", package.name));
            return false;
        }
    }
    return true;
}

bool DistributedPackageRegistry::Publish(const DistributedPackage& package, ea::string* error)
{
    if (!ValidatePackage(package, error))
        return false;
    for (const DistributedPackage& existing : packages_)
    {
        if (existing.name == package.name && existing.version == package.version && existing.platform == package.platform
            && existing.architecture == package.architecture)
        {
            SetError(error, Format("Package '{}' version '{}' is already published for this target.", package.name, package.version));
            return false;
        }
    }
    packages_.push_back(package);
    std::sort(packages_.begin(), packages_.end(), [](const auto& lhs, const auto& rhs)
    {
        if (lhs.name != rhs.name) return lhs.name < rhs.name;
        if (lhs.platform != rhs.platform) return static_cast<unsigned>(lhs.platform) < static_cast<unsigned>(rhs.platform);
        if (lhs.architecture != rhs.architecture) return lhs.architecture < rhs.architecture;
        return CompareVersions(lhs.version, rhs.version) > 0;
    });
    return true;
}

bool DistributedPackageRegistry::RemovePackage(const ea::string& name, const ea::string& version,
    PackagePlatform platform, const ea::string& architecture, ea::string* error)
{
    auto it = std::find_if(packages_.begin(), packages_.end(), [&](const auto& package)
    {
        return package.name == name && package.version == version && package.platform == platform && package.architecture == architecture;
    });
    if (it == packages_.end())
    {
        SetError(error, Format("Package '{}' version '{}' is not published for this target.", name, version));
        return false;
    }
    packages_.erase(it);
    return true;
}

const DistributedPackage* DistributedPackageRegistry::Resolve(const PackageResolveRequest& request, ea::string* error) const
{
    const DistributedPackage* best = nullptr;
    for (const DistributedPackage& package : packages_)
    {
        if (package.name != request.name || package.platform != request.platform || package.architecture != request.architecture
            || !MatchesVersion(package.version, request.versionRange))
            continue;
        if (!best || CompareVersions(package.version, best->version) > 0
            || (package.version == best->version && package.digest < best->digest))
            best = &package;
    }
    if (!best)
        SetError(error, Format("No package '{}' matches version '{}' for the requested target.", request.name, request.versionRange));
    return best;
}

bool DistributedPackageRegistry::BuildReplicationPlan(const ea::string& packageDigest,
    ea::vector<PackageReplicationTarget>& plan, ea::string* error) const
{
    plan.clear();
    if (packageDigest.empty())
    {
        SetError(error, "Replication requires a non-empty package digest.");
        return false;
    }
    bool found = false;
    for (const DistributedPackage& package : packages_)
    {
        if (package.digest == packageDigest)
        {
            found = true;
            break;
        }
    }
    if (!found)
    {
        SetError(error, Format("Package digest '{}' is not indexed.", packageDigest));
        return false;
    }
    for (const PackageRepositoryEndpoint& repository : repositories_)
    {
        if (repository.writable)
            plan.push_back({repository.id, packageDigest});
    }
    if (plan.empty())
    {
        SetError(error, "Replication requires at least one writable repository.");
        return false;
    }
    return true;
}

bool DistributedPackageRegistry::Validate(ea::string* error) const
{
    for (unsigned i = 0; i < repositories_.size(); ++i)
    {
        if (repositories_[i].id.empty() || repositories_[i].url.empty())
        {
            SetError(error, "Package repositories must have non-empty identifiers and URLs.");
            return false;
        }
        for (unsigned j = i + 1; j < repositories_.size(); ++j)
        {
            if (repositories_[i].id == repositories_[j].id)
            {
                SetError(error, Format("Package repository '{}' must be unique.", repositories_[i].id));
                return false;
            }
        }
    }
    for (unsigned i = 0; i < packages_.size(); ++i)
    {
        if (!ValidatePackage(packages_[i], error))
            return false;
        for (unsigned j = i + 1; j < packages_.size(); ++j)
        {
            if (packages_[i].name == packages_[j].name && packages_[i].version == packages_[j].version
                && packages_[i].platform == packages_[j].platform && packages_[i].architecture == packages_[j].architecture)
            {
                SetError(error, Format("Package identity '{}' '{}' is duplicated.", packages_[i].name, packages_[i].version));
                return false;
            }
        }
    }
    return true;
}

JSONValue DistributedPackageRegistry::ToJSON() const
{
    JSONValue root(JSON_OBJECT);
    root.Set("version", 1u);

    JSONValue repositories(JSON_ARRAY);
    ea::vector<PackageRepositoryEndpoint> sortedRepositories = repositories_;
    std::sort(sortedRepositories.begin(), sortedRepositories.end(), [](const auto& lhs, const auto& rhs)
    {
        return lhs.id < rhs.id;
    });
    for (const auto& repository : sortedRepositories)
    {
        JSONValue item(JSON_OBJECT);
        item.Set("id", repository.id);
        item.Set("url", repository.url);
        item.Set("priority", repository.priority);
        item.Set("writable", repository.writable);
        repositories.Push(ea::move(item));
    }
    root.Set("repositories", ea::move(repositories));

    JSONValue packages(JSON_ARRAY);
    ea::vector<DistributedPackage> sortedPackages = packages_;
    std::sort(sortedPackages.begin(), sortedPackages.end(), [](const auto& lhs, const auto& rhs)
    {
        if (lhs.name != rhs.name) return lhs.name < rhs.name;
        if (lhs.version != rhs.version) return CompareVersions(lhs.version, rhs.version) > 0;
        if (lhs.platform != rhs.platform) return static_cast<unsigned>(lhs.platform) < static_cast<unsigned>(rhs.platform);
        return lhs.architecture < rhs.architecture;
    });
    for (const auto& package : sortedPackages)
    {
        JSONValue item(JSON_OBJECT);
        item.Set("name", package.name);
        item.Set("version", package.version);
        item.Set("platform", PackageBuilder::ToString(package.platform));
        item.Set("architecture", package.architecture);
        item.Set("digest", package.digest);
        item.Set("size", Format("{}", package.size));
        item.Set("artifact", package.artifact);
        JSONValue dependencies(JSON_ARRAY);
        ea::vector<ea::string> sortedDependencies = package.dependencies;
        std::sort(sortedDependencies.begin(), sortedDependencies.end());
        for (const auto& dependency : sortedDependencies)
            dependencies.Push(dependency);
        item.Set("dependencies", ea::move(dependencies));
        packages.Push(ea::move(item));
    }
    root.Set("packages", ea::move(packages));
    return root;
}

bool DistributedPackageRegistry::FromJSON(const JSONValue& root, ea::string* error)
{
    if (!root.IsObject() || !root.Contains("version") || root["version"].GetUInt() != 1u
        || !root.Contains("repositories") || !root["repositories"].IsArray()
        || !root.Contains("packages") || !root["packages"].IsArray())
    {
        SetError(error, "Distributed package registry requires version 1, repositories and packages arrays.");
        return false;
    }

    DistributedPackageRegistry candidate(context_);
    for (const JSONValue& value : root["repositories"].GetArray())
    {
        if (!value.IsObject() || !value.Contains("id") || !value.Contains("url") || !value.Contains("priority") || !value.Contains("writable"))
        {
            SetError(error, "Distributed package registry contains an invalid repository.");
            return false;
        }
        PackageRepositoryEndpoint repository;
        repository.id = value["id"].GetString();
        repository.url = value["url"].GetString();
        repository.priority = value["priority"].GetInt();
        repository.writable = value["writable"].GetBool();
        if (!candidate.AddRepository(repository, error))
            return false;
    }
    for (const JSONValue& value : root["packages"].GetArray())
    {
        if (!value.IsObject() || !value.Contains("name") || !value.Contains("version") || !value.Contains("platform")
            || !value.Contains("architecture") || !value.Contains("digest") || !value.Contains("size") || !value.Contains("artifact")
            || !value.Contains("dependencies") || !value["dependencies"].IsArray())
        {
            SetError(error, "Distributed package registry contains an invalid package.");
            return false;
        }
        DistributedPackage package;
        package.name = value["name"].GetString();
        package.version = value["version"].GetString();
        if (!PackageBuilder::FromString(value["platform"].GetString(), package.platform))
        {
            SetError(error, "Distributed package registry contains an unsupported platform.");
            return false;
        }
        package.architecture = value["architecture"].GetString();
        package.digest = value["digest"].GetString();
        package.artifact = value["artifact"].GetString();
        if (value["size"].IsString())
        {
            if (!ParseUnsigned(value["size"].GetString(), package.size))
            {
                SetError(error, Format("Invalid package size for '{}'.", package.name));
                return false;
            }
        }
        else
            package.size = static_cast<unsigned long long>(value["size"].GetDouble());
        for (const JSONValue& dependency : value["dependencies"].GetArray())
        {
            if (!dependency.IsString())
            {
                SetError(error, Format("Package '{}' contains a non-string dependency.", package.name));
                return false;
            }
            package.dependencies.push_back(dependency.GetString());
        }
        if (!candidate.Publish(package, error))
            return false;
    }
    if (!candidate.Validate(error))
        return false;
    packages_ = ea::move(candidate.packages_);
    repositories_ = ea::move(candidate.repositories_);
    return true;
}

unsigned long long DistributedPackageRegistry::ComputeDigest(ea::string* error) const
{
    if (!Validate(error))
        return 0;
    JSONFile jsonFile(context_);
    jsonFile.GetRoot() = ToJSON();
    const ea::string serialized = jsonFile.ToString("");
    unsigned long long digest = 1469598103934665603ull;
    for (const unsigned char character : serialized)
    {
        digest ^= character;
        digest *= 1099511628211ull;
    }
    return digest;
}

void DistributedPackageRegistry::SerializeInBlock(Archive& archive)
{
    (void)archive;
}

bool DistributedPackageRegistry::BeginLoad(Deserializer& source)
{
    JSONFile jsonFile(context_);
    if (!jsonFile.Load(source))
        return false;
    return FromJSON(jsonFile.GetRoot());
}

bool DistributedPackageRegistry::Save(Serializer& dest) const
{
    JSONFile jsonFile(context_);
    jsonFile.GetRoot() = ToJSON();
    return jsonFile.Save(dest);
}

} // namespace Urho3D
