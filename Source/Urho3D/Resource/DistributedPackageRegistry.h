// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <Urho3D/Resource/JSONValue.h>
#include <Urho3D/Resource/Resource.h>
#include <Urho3D/Resource/PackageBuilder.h>

#include <EASTL/vector.h>

namespace Urho3D
{

struct URHO3D_API DistributedPackage
{
    ea::string name;
    ea::string version;
    PackagePlatform platform{PackagePlatform::Linux};
    ea::string architecture;
    ea::string digest;
    unsigned long long size{};
    ea::string artifact;
    ea::vector<ea::string> dependencies;
};

struct URHO3D_API PackageRepositoryEndpoint
{
    ea::string id;
    ea::string url;
    int priority{};
    bool writable{};
};

struct URHO3D_API PackageResolveRequest
{
    ea::string name;
    ea::string versionRange{"*"};
    PackagePlatform platform{PackagePlatform::Linux};
    ea::string architecture;
};

struct URHO3D_API PackageReplicationTarget
{
    ea::string repositoryId;
    ea::string packageDigest;
};

/// Transport-independent distributed package index.
/// It validates immutable package identities and produces deterministic resolution
/// and replication plans for HTTP, object storage or an enterprise package service.
class URHO3D_API DistributedPackageRegistry : public Resource
{
    URHO3D_OBJECT(DistributedPackageRegistry, Resource);

public:
    explicit DistributedPackageRegistry(Context* context);

    static bool CheckExtension(const ea::string& fileName);

    void Reset();
    bool AddRepository(const PackageRepositoryEndpoint& repository, ea::string* error = nullptr);
    bool RemoveRepository(const ea::string& repositoryId, ea::string* error = nullptr);
    bool Publish(const DistributedPackage& package, ea::string* error = nullptr);
    bool RemovePackage(const ea::string& name, const ea::string& version, PackagePlatform platform,
        const ea::string& architecture, ea::string* error = nullptr);

    const ea::vector<DistributedPackage>& GetPackages() const { return packages_; }
    const ea::vector<PackageRepositoryEndpoint>& GetRepositories() const { return repositories_; }
    const DistributedPackage* Resolve(const PackageResolveRequest& request, ea::string* error = nullptr) const;
    bool BuildReplicationPlan(const ea::string& packageDigest, ea::vector<PackageReplicationTarget>& plan,
        ea::string* error = nullptr) const;

    bool Validate(ea::string* error = nullptr) const;
    unsigned long long ComputeDigest(ea::string* error = nullptr) const;
    JSONValue ToJSON() const;
    bool FromJSON(const JSONValue& root, ea::string* error = nullptr);

    void SerializeInBlock(Archive& archive) override;
    bool BeginLoad(Deserializer& source) override;
    bool Save(Serializer& dest) const override;

private:
    static bool IsValidVersion(const ea::string& version);
    static bool MatchesVersion(const ea::string& version, const ea::string& range);
    static int CompareVersions(const ea::string& lhs, const ea::string& rhs);
    bool ValidatePackage(const DistributedPackage& package, ea::string* error) const;
    void SetError(ea::string* error, const ea::string& message) const;

    ea::vector<DistributedPackage> packages_;
    ea::vector<PackageRepositoryEndpoint> repositories_;
    mutable ea::string lastError_;
};

} // namespace Urho3D
