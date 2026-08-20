#include <Urho3D/Resource/PackageBuilder.h>
#include <Urho3D/Resource/PlatformExportAdapter.h>

#include <algorithm>
#include <catch2/catch_amalgamated.hpp>

using namespace Urho3D;

TEST_CASE("Package build profile round trip preserves platform and filters", "[packaging][profile]")
{
    PackageBuildProfile profile;
    profile.version = 3;
    profile.name = "WindowsShipping";
    profile.platform = PackagePlatform::Windows;
    profile.architecture = "x64";
    profile.optimization = PackageOptimization::Shipping;
    profile.outputPath = "dist/windows";
    profile.reproducible = true;
    profile.assetFilters.push_back({"*.rbscene", false});
    profile.assetFilters.push_back({"Generated/*", true});

    const JSONValue json = profile.ToJSON();
    PackageBuildProfile loaded;
    ea::string error;
    REQUIRE(loaded.FromJSON(json, &error));
    CHECK(error.empty());
    CHECK(loaded.version == 3);
    CHECK(loaded.name == "WindowsShipping");
    CHECK(loaded.platform == PackagePlatform::Windows);
    CHECK(loaded.architecture == "x64");
    CHECK(loaded.optimization == PackageOptimization::Shipping);
    CHECK(loaded.outputPath == "dist/windows");
    CHECK(loaded.IncludesAsset("levels/main.rbscene"));
    CHECK_FALSE(loaded.IncludesAsset("Generated/main.rbscene"));
    CHECK_FALSE(loaded.IncludesAsset("textures/albedo.png"));
}

TEST_CASE("Package filters include all assets when no include rule exists", "[packaging][filters]")
{
    PackageBuildProfile profile;
    profile.assetFilters.push_back({"Generated/*", true});
    CHECK(profile.IncludesAsset("Textures/albedo.png"));
    CHECK_FALSE(profile.IncludesAsset("Generated/cache.bin"));
}

TEST_CASE("Package builder applies filters and produces a sorted manifest", "[packaging][manifest]")
{
    PackageBuildProfile profile;
    profile.name = "LinuxDevelopment";
    profile.platform = PackagePlatform::Linux;
    profile.architecture = "x64";
    profile.assetFilters.push_back({"*", false});
    profile.assetFilters.push_back({"*.tmp", true});

    ea::vector<PackageFileEntry> candidates;
    candidates.push_back({"z.rbscene", "Content/z.rbscene", 10, 100});
    candidates.push_back({"cache.tmp", "Content/cache.tmp", 11, 20});
    candidates.push_back({"a.rbscene", "Content/a.rbscene", 12, 200});

    PackageManifest manifest;
    ea::string error;
    REQUIRE(PackageBuilder::BuildManifest(profile, candidates, manifest, &error));
    CHECK(error.empty());
    REQUIRE(manifest.files.size() == 2);
    CHECK(manifest.files[0].sourcePath == "z.rbscene");
    CHECK(manifest.files[1].sourcePath == "a.rbscene");

    const JSONValue json = manifest.ToJSON();
    REQUIRE(json.Contains("files"));
    REQUIRE(json["files"].GetArray().size() == 2);
    CHECK(json["files"][0]["packagePath"].GetString() == "Content/a.rbscene");
    CHECK(json["files"][1]["packagePath"].GetString() == "Content/z.rbscene");

    PackageManifest loaded;
    REQUIRE(loaded.FromJSON(json, &error));
    CHECK(loaded.files.size() == 2);
    CHECK(loaded.files[0].size == 200);
}

TEST_CASE("Package manifest validation rejects duplicate output paths", "[packaging][validation]")
{
    PackageManifest manifest;
    manifest.profileName = "Linux";
    manifest.architecture = "x64";
    manifest.files.push_back({"a.png", "Content/shared.bin", 1, 10});
    manifest.files.push_back({"b.png", "Content/shared.bin", 2, 20});

    const PackageValidationResult validation = PackageBuilder::ValidateManifest(manifest);
    CHECK_FALSE(validation.valid);
    REQUIRE_FALSE(validation.errors.empty());
    CHECK(validation.errors.front().find("Duplicate") != ea::string::npos);
}

TEST_CASE("Package manifest preserves the World Fabric digest", "[packaging][worldfabric]")
{
    PackageBuildProfile profile;
    profile.name = "LinuxWorldFabric";
    profile.platform = PackagePlatform::Linux;
    profile.architecture = "x64";
    profile.worldFabricDigest = 0x123456789abcdef0ULL;

    PackageManifest manifest;
    ea::string error;
    REQUIRE(PackageBuilder::BuildManifest(profile, {}, manifest, &error));
    CHECK(error.empty());
    CHECK(manifest.worldFabricDigest == profile.worldFabricDigest);

    const JSONValue json = manifest.ToJSON();
    CHECK(json["worldFabricDigest"].GetString() == "1311768467463790320");

    PackageManifest loaded;
    REQUIRE(loaded.FromJSON(json, &error));
    CHECK(loaded.worldFabricDigest == profile.worldFabricDigest);
}

TEST_CASE("Platform export adapter rejects an incompatible architecture", "[packaging][platform]")
{
    PackageBuildProfile profile;
    profile.platform = PackagePlatform::WebAssembly;
    profile.architecture = "x64";
    profile.outputPath = "Build/web";
    const PlatformExportAdapter* adapter = PlatformExportAdapter::Find(profile.platform);
    REQUIRE(adapter != nullptr);

    ea::string error;
    CHECK_FALSE(adapter->Validate(profile, &error));
    const bool mentionsArchitecture = error.find("architecture") != ea::string::npos
        || error.find("Architecture") != ea::string::npos;
    CHECK(mentionsArchitecture);
}


TEST_CASE("Package manifest records recipe cache and artifact provenance", "[packaging][provenance]")
{
    PackageBuildProfile profile;
    profile.name = "LinuxShipping";
    profile.platform = PackagePlatform::Linux;
    profile.architecture = "x64";
    profile.buildGraphDigest = "buildgraph-7f3a";
    profile.assetCacheDigest = "assetcache-19c2";

    PackageFileEntry mesh;
    mesh.sourcePath = "Models/arena.glb";
    mesh.packagePath = "Content/Models/arena.glb";
    mesh.contentHash = 0x1234u;
    mesh.size = 4096;
    mesh.contentDigest = "sha256:arena";
    mesh.importProfileDigest = "model-profile:competitive";
    mesh.provenance = "BuildGraph/CookModel/arena";

    PackageFileEntry script;
    script.sourcePath = "Scripts/match.rbscript";
    script.packagePath = "Content/Scripts/match.rbscript";
    script.contentHash = 0x5678u;
    script.size = 128;
    script.contentDigest = "sha256:match";
    script.provenance = "BuildGraph/CompileRbScript/match";

    ea::vector<PackageFileEntry> candidates;
    candidates.push_back(script);
    candidates.push_back(mesh);

    PackageManifest manifest;
    ea::string error;
    REQUIRE(PackageBuilder::BuildManifest(profile, candidates, manifest, &error));
    CHECK(error.empty());
    CHECK(manifest.buildGraphDigest == profile.buildGraphDigest);
    CHECK(manifest.assetCacheDigest == profile.assetCacheDigest);
    CHECK_FALSE(manifest.provenanceDigest.empty());
    CHECK(manifest.provenanceDigest == Format("{}", manifest.ComputeDigest()));

    const JSONValue json = manifest.ToJSON();
    CHECK(json["buildGraphDigest"].GetString() == "buildgraph-7f3a");
    CHECK(json["assetCacheDigest"].GetString() == "assetcache-19c2");
    REQUIRE(json["files"].GetArray().size() == 2);
    CHECK(json["files"][0]["contentDigest"].GetString() == "sha256:arena");
    CHECK(json["files"][0]["importProfileDigest"].GetString() == "model-profile:competitive");
    CHECK(json["files"][0]["provenance"].GetString() == "BuildGraph/CookModel/arena");

    PackageManifest loaded;
    REQUIRE(loaded.FromJSON(json, &error));
    CHECK(loaded.ComputeDigest() == manifest.ComputeDigest());
    const auto loadedMesh = std::find_if(loaded.files.begin(), loaded.files.end(), [](const PackageFileEntry& file)
    {
        return file.packagePath == "Content/Models/arena.glb";
    });
    REQUIRE(loadedMesh != loaded.files.end());
    CHECK(loadedMesh->provenance == "BuildGraph/CookModel/arena");
}

TEST_CASE("Package manifest digest is independent of candidate insertion order", "[packaging][provenance]")
{
    PackageManifest first;
    first.profileName = "WindowsShipping";
    first.platform = PackagePlatform::Windows;
    first.architecture = "x64";
    first.buildGraphDigest = "graph";
    first.assetCacheDigest = "cache";
    first.files.push_back({"z.bin", "Content/z.bin", 2, 20, "digest-z", "profile-z", "cook-z"});
    first.files.push_back({"a.bin", "Content/a.bin", 1, 10, "digest-a", "profile-a", "cook-a"});

    PackageManifest second = first;
    second.files.clear();
    second.files.push_back(first.files[1]);
    second.files.push_back(first.files[0]);

    REQUIRE(PackageBuilder::ValidateManifest(first).valid);
    REQUIRE(PackageBuilder::ValidateManifest(second).valid);
    CHECK(first.ComputeDigest() == second.ComputeDigest());
}
