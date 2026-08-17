// SPDX-License-Identifier: MIT

#include <Urho3D/IO/VectorBuffer.h>
#include <Urho3D/WorldFabric/BuildDashboardResource.h>

#include "CommonUtils.h"

#include <catch2/catch_amalgamated.hpp>

using namespace Urho3D;

TEST_CASE("Build Dashboard resource round-trips deterministic manifests", "[buildgraph][resource]")
{
    const auto context = Tests::GetOrCreateContext(Tests::CreateCompleteContext);
    BuildDashboardResource source(context.Get());
    source.SetPlatform("Windows");
    source.SetConfiguration("Shipping");

    BuildTask import;
    import.key = "ImportAssets";
    import.kind = BuildTaskKind::ImportAsset;
    import.metadata["path"] = Variant(ea::string("Assets"));

    BuildTask shader;
    shader.key = "CompileShaders";
    shader.kind = BuildTaskKind::CompileShader;
    shader.metadata["profile"] = Variant(ea::string("SM6"));
    shader.dependencies.push_back(import.key);

    BuildTask package;
    package.key = "PackageGame";
    package.kind = BuildTaskKind::BuildPackage;
    package.metadata["target"] = Variant(ea::string("Windows-x64"));
    package.dependencies.push_back(shader.key);

    source.GetTasks() = {import, shader, package};

    const JSONValue serialized = source.ToJSON();
    BuildDashboardResource restored(context.Get());
    ea::string error;
    REQUIRE(restored.FromJSON(serialized, &error));
    CHECK(error.empty());
    CHECK(restored.GetPlatform() == "Windows");
    CHECK(restored.GetConfiguration() == "Shipping");
    REQUIRE(restored.GetTasks().size() == 3);
    CHECK(restored.GetTasks()[1].metadata.at("profile").GetString() == "SM6");

    const ea::vector<ea::string> expectedOrder{"ImportAssets", "CompileShaders", "PackageGame"};
    CHECK(restored.GetBuildOrder(&error) == expectedOrder);
    CHECK(error.empty());
    CHECK(restored.ComputeDigest(&error) != 0);
    CHECK(error.empty());

    VectorBuffer buffer;
    buffer.SetName("BuildDashboard.builddashboard");
    REQUIRE(source.Save(buffer));
    REQUIRE(buffer.GetSize() > 0);
    buffer.Seek(0);
    BuildDashboardResource loaded(context.Get());
    REQUIRE(loaded.BeginLoad(buffer));
    CHECK(loaded.GetTasks().size() == 3);
    CHECK(loaded.GetBuildOrder() == expectedOrder);

    CHECK(BuildDashboardResource::CheckExtension("Desktop.builddashboard"));
    CHECK(BuildDashboardResource::CheckExtension("Desktop.buildgraph"));
    CHECK_FALSE(BuildDashboardResource::CheckExtension("Desktop.json"));
    CHECK_FALSE(BuildDashboardResource::CheckExtension("Desktop.buildgraph.bak"));

    JSONValue missingTasks = serialized;
    missingTasks.Erase("tasks");
    REQUIRE_FALSE(restored.FromJSON(missingTasks, &error));
    CHECK_FALSE(error.empty());

    JSONValue invalidKind = serialized;
    invalidKind["tasks"][0]["kind"] = "NotARealBuildTask";
    REQUIRE_FALSE(restored.FromJSON(invalidKind, &error));
    CHECK_FALSE(error.empty());

    JSONValue duplicateTask = serialized;
    JSONValue duplicateTasks = duplicateTask["tasks"];
    duplicateTasks.Push(duplicateTasks[0]);
    duplicateTask["tasks"] = duplicateTasks;
    REQUIRE_FALSE(restored.FromJSON(duplicateTask, &error));
    CHECK_FALSE(error.empty());

    JSONValue cyclic = serialized;
    cyclic["tasks"][0]["dependencies"].Push("PackageGame");
    REQUIRE_FALSE(restored.FromJSON(cyclic, &error));
    CHECK_FALSE(error.empty());
}
