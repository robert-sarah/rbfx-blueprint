#include <Urho3D/IO/VectorBuffer.h>
#include <Urho3D/WorldFabric/WorldFabricGraphResource.h>

#include "CommonUtils.h"

#include <catch2/catch_amalgamated.hpp>

using namespace Urho3D;

TEST_CASE("World Fabric graph resource round-trips deterministic semantic dependencies", "[worldfabric][resource]")
{
    const auto context = Tests::GetOrCreateContext(Tests::CreateCompleteContext);
    WorldFabricGraphResource source(context.Get());

    StringVariantMap metadata;
    metadata["owner"] = Variant(ea::string("gameplay"));
    metadata["critical"] = Variant(true);

    const WorldFabricId asset = source.GetGraph().AddNode("asset/player", WorldFabricNodeKind::Asset, "Model", metadata);
    const WorldFabricId blueprint = source.GetGraph().AddNode("blueprint/player", WorldFabricNodeKind::Blueprint, "PlayerBlueprint");
    const WorldFabricId entity = source.GetGraph().AddNode("entity/player", WorldFabricNodeKind::Entity, "PlayerEntity");
    REQUIRE(asset != InvalidWorldFabricId);
    REQUIRE(blueprint != InvalidWorldFabricId);
    REQUIRE(entity != InvalidWorldFabricId);
    REQUIRE(source.GetGraph().AddDependency(blueprint, asset, WorldFabricDependencyKind::BuildsFrom, "mesh"));
    REQUIRE(source.GetGraph().AddDependency(entity, blueprint, WorldFabricDependencyKind::Requires, "spawn"));

    ea::string error;
    const JSONValue serialized = source.ToJSON();
    WorldFabricGraphResource restored(context.Get());
    REQUIRE(restored.FromJSON(serialized, &error));
    CHECK(error.empty());
    CHECK(restored.Validate(&error));
    CHECK(error.empty());
    CHECK(restored.GetGraph().ComputeDigest() == source.GetGraph().ComputeDigest());

    const WorldFabricNode* restoredAsset = restored.GetGraph().GetNode(asset);
    REQUIRE(restoredAsset);
    CHECK(restoredAsset->type == "Model");
    REQUIRE(restoredAsset->metadata.find("owner") != restoredAsset->metadata.end());
    CHECK(restoredAsset->metadata.at("owner").GetString() == "gameplay");
    CHECK(restored.GetGraph().GetDependencies(blueprint).size() == 1);
    CHECK(restored.GetGraph().GetDependencies(blueprint).front().label == "mesh");

    const ea::vector<WorldFabricId> order = restored.GetBuildOrder(&error);
    REQUIRE(error.empty());
    REQUIRE(order.size() == 3);
    CHECK(ea::find(order.begin(), order.end(), asset) < ea::find(order.begin(), order.end(), blueprint));
    CHECK(ea::find(order.begin(), order.end(), blueprint) < ea::find(order.begin(), order.end(), entity));

    VectorBuffer buffer;
    buffer.SetName("Gameplay.worldfabric");
    REQUIRE(source.Save(buffer));
    REQUIRE(buffer.GetSize() > 0);
    buffer.Seek(0);
    WorldFabricGraphResource loaded(context.Get());
    REQUIRE(loaded.BeginLoad(buffer));
    CHECK(loaded.GetGraph().ComputeDigest() == source.GetGraph().ComputeDigest());
    CHECK(loaded.GetGraph().GetNodes().size() == 3);

    CHECK(WorldFabricGraphResource::CheckExtension("Gameplay.worldfabric"));
    CHECK(WorldFabricGraphResource::CheckExtension("Gameplay.fabric"));
    CHECK(WorldFabricGraphResource::CheckExtension("Gameplay.worldfabricgraph"));
    CHECK_FALSE(WorldFabricGraphResource::CheckExtension("Gameplay.json"));
    CHECK_FALSE(WorldFabricGraphResource::CheckExtension("Gameplay.worldfabric.bak"));

    JSONValue missingNodes = serialized;
    missingNodes.Erase("nodes");
    REQUIRE_FALSE(restored.FromJSON(missingNodes, &error));
    CHECK_FALSE(error.empty());

    JSONValue invalidKind = serialized;
    invalidKind["nodes"][0]["kind"] = "NotARealWorldFabricKind";
    REQUIRE_FALSE(restored.FromJSON(invalidKind, &error));
    CHECK_FALSE(error.empty());

    JSONValue unknownEdge = serialized;
    unknownEdge["edges"][0]["dependency"] = "asset/missing";
    REQUIRE_FALSE(restored.FromJSON(unknownEdge, &error));
    CHECK_FALSE(error.empty());

    JSONValue cyclic = serialized;
    JSONValue cyclicEdges = cyclic["edges"];
    JSONValue cycle(JSON_OBJECT);
    cycle.Set("node", "asset/player");
    cycle.Set("dependency", "entity/player");
    cycle.Set("kind", "Requires");
    cycle.Set("label", "cycle");
    cyclicEdges.Push(ea::move(cycle));
    cyclic["edges"] = ea::move(cyclicEdges);
    REQUIRE_FALSE(restored.FromJSON(cyclic, &error));
    CHECK_FALSE(error.empty());
}
