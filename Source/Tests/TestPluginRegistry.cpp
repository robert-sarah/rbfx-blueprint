// SPDX-License-Identifier: MIT

#include <Urho3D/IO/VectorBuffer.h>
#include <Urho3D/WorldFabric/PluginRegistry.h>

#include "CommonUtils.h"

#include <catch2/catch_amalgamated.hpp>

using namespace Urho3D;

TEST_CASE("Plugin registry round-trips deterministic SDK manifests", "[plugins][registry]")
{
    const auto context = Tests::GetOrCreateContext(Tests::CreateCompleteContext);
    PluginRegistry source(context.Get());

    PluginDescriptor renderer;
    renderer.name = "Runtime.Renderer";
    renderer.version = "1.2.0";
    renderer.entryPoint = "RendererPlugin";
    renderer.binary = "Plugins/Renderer";
    renderer.capabilities = {"Rendering", "WorldFabric"};
    renderer.metadata["owner"] = Variant(ea::string("engine"));

    PluginDescriptor gameplay;
    gameplay.name = "Runtime.Gameplay";
    gameplay.version = "2.0.1-beta.1";
    gameplay.entryPoint = "GameplayPlugin";
    gameplay.capabilities = {"Gameplay", "Scripting"};
    gameplay.dependencies = {renderer.name};

    REQUIRE(source.AddPlugin(gameplay));
    REQUIRE(source.AddPlugin(renderer));

    ea::string error;
    REQUIRE(source.Validate(&error));
    CHECK(error.empty());
    CHECK(source.GetLoadOrder(&error) == ea::vector<ea::string>{renderer.name, gameplay.name});
    CHECK(error.empty());
    const unsigned long long digest = source.ComputeDigest(&error);
    REQUIRE(digest != 0);
    CHECK(error.empty());

    const JSONValue serialized = source.ToJSON();
    PluginRegistry restored(context.Get());
    REQUIRE(restored.FromJSON(serialized, &error));
    CHECK(error.empty());
    REQUIRE(restored.GetPlugins().size() == 2);
    CHECK(restored.FindPlugin(renderer.name)->metadata.at("owner").GetString() == "engine");
    CHECK(restored.GetLoadOrder(&error) == source.GetLoadOrder(&error));
    CHECK(restored.ComputeDigest(&error) == digest);

    VectorBuffer buffer;
    buffer.SetName("Runtime.pluginregistry");
    REQUIRE(source.Save(buffer));
    REQUIRE(buffer.GetSize() > 0);
    buffer.Seek(0);
    PluginRegistry loaded(context.Get());
    REQUIRE(loaded.BeginLoad(buffer));
    CHECK(loaded.GetLoadOrder() == ea::vector<ea::string>{renderer.name, gameplay.name});

    CHECK(PluginRegistry::CheckExtension("Runtime.pluginregistry"));
    CHECK(PluginRegistry::CheckExtension("Runtime.plugins"));
    CHECK(PluginRegistry::CheckExtension("Runtime.pluginmanifest"));
    CHECK_FALSE(PluginRegistry::CheckExtension("Runtime.json"));
    CHECK_FALSE(PluginRegistry::CheckExtension("Runtime.plugins.bak"));

    JSONValue missingPlugins = serialized;
    missingPlugins.Erase("plugins");
    REQUIRE_FALSE(restored.FromJSON(missingPlugins, &error));
    CHECK_FALSE(error.empty());

    JSONValue missingDependency = serialized;
    missingDependency["plugins"][0]["dependencies"][0] = "Runtime.DoesNotExist";
    REQUIRE_FALSE(restored.FromJSON(missingDependency, &error));
    CHECK_FALSE(error.empty());

    JSONValue duplicateCapability = serialized;
    duplicateCapability["plugins"][0]["capabilities"].Push("Scripting");
    duplicateCapability["plugins"][0]["capabilities"].Push("Scripting");
    REQUIRE_FALSE(restored.FromJSON(duplicateCapability, &error));
    CHECK_FALSE(error.empty());

    JSONValue cycle = serialized;
    cycle["plugins"][1]["dependencies"].Push("Runtime.Gameplay");
    REQUIRE_FALSE(restored.FromJSON(cycle, &error));
    CHECK_FALSE(error.empty());

    JSONValue invalidVersion = serialized;
    invalidVersion["plugins"][0]["version"] = "release";
    REQUIRE_FALSE(restored.FromJSON(invalidVersion, &error));
    CHECK_FALSE(error.empty());
}
