#include "../Precompiled.h"

#include "WorldFabricGraphResource.h"

#include "WorldFabricReflection.h"

#include "../IO/Deserializer.h"
#include "../IO/Serializer.h"
#include "../Resource/JSONFile.h"

namespace Urho3D
{

WorldFabricGraphResource::WorldFabricGraphResource(Context* context)
    : Resource(context)
{
}

bool WorldFabricGraphResource::CheckExtension(const ea::string& fileName)
{
    return fileName.ends_with(".worldfabric", false) || fileName.ends_with(".fabric", false)
        || fileName.ends_with(".worldfabricgraph", false);
}

const char* WorldFabricGraphResource::GetNodeKindName(WorldFabricNodeKind kind)
{
    switch (kind)
    {
    case WorldFabricNodeKind::Entity: return "Entity";
    case WorldFabricNodeKind::Component: return "Component";
    case WorldFabricNodeKind::Blueprint: return "Blueprint";
    case WorldFabricNodeKind::RbScript: return "RbScript";
    case WorldFabricNodeKind::Asset: return "Asset";
    case WorldFabricNodeKind::SceneCell: return "SceneCell";
    case WorldFabricNodeKind::RenderResource: return "RenderResource";
    case WorldFabricNodeKind::NetworkObject: return "NetworkObject";
    case WorldFabricNodeKind::AudioBus: return "AudioBus";
    case WorldFabricNodeKind::Animation: return "Animation";
    case WorldFabricNodeKind::Custom: return "Custom";
    default: return "Custom";
    }
}

bool WorldFabricGraphResource::ParseNodeKind(const ea::string& name, WorldFabricNodeKind& kind)
{
    if (name == "Entity") kind = WorldFabricNodeKind::Entity;
    else if (name == "Component") kind = WorldFabricNodeKind::Component;
    else if (name == "Blueprint") kind = WorldFabricNodeKind::Blueprint;
    else if (name == "RbScript") kind = WorldFabricNodeKind::RbScript;
    else if (name == "Asset") kind = WorldFabricNodeKind::Asset;
    else if (name == "SceneCell") kind = WorldFabricNodeKind::SceneCell;
    else if (name == "RenderResource") kind = WorldFabricNodeKind::RenderResource;
    else if (name == "NetworkObject") kind = WorldFabricNodeKind::NetworkObject;
    else if (name == "AudioBus") kind = WorldFabricNodeKind::AudioBus;
    else if (name == "Animation") kind = WorldFabricNodeKind::Animation;
    else if (name == "Custom") kind = WorldFabricNodeKind::Custom;
    else return false;
    return true;
}

const char* WorldFabricGraphResource::GetDependencyKindName(WorldFabricDependencyKind kind)
{
    switch (kind)
    {
    case WorldFabricDependencyKind::Requires: return "Requires";
    case WorldFabricDependencyKind::Produces: return "Produces";
    case WorldFabricDependencyKind::References: return "References";
    case WorldFabricDependencyKind::Replicates: return "Replicates";
    case WorldFabricDependencyKind::StreamsWith: return "StreamsWith";
    case WorldFabricDependencyKind::Profiles: return "Profiles";
    case WorldFabricDependencyKind::BuildsFrom: return "BuildsFrom";
    default: return "Requires";
    }
}

bool WorldFabricGraphResource::ParseDependencyKind(const ea::string& name, WorldFabricDependencyKind& kind)
{
    if (name == "Requires") kind = WorldFabricDependencyKind::Requires;
    else if (name == "Produces") kind = WorldFabricDependencyKind::Produces;
    else if (name == "References") kind = WorldFabricDependencyKind::References;
    else if (name == "Replicates") kind = WorldFabricDependencyKind::Replicates;
    else if (name == "StreamsWith") kind = WorldFabricDependencyKind::StreamsWith;
    else if (name == "Profiles") kind = WorldFabricDependencyKind::Profiles;
    else if (name == "BuildsFrom") kind = WorldFabricDependencyKind::BuildsFrom;
    else return false;
    return true;
}

const WorldFabricNode* WorldFabricGraphResource::FindNodeByKey(
    const WorldFabricGraph& graph, const ea::string& key) const
{
    const ea::vector<WorldFabricNode> nodes = graph.GetNodes();
    for (const WorldFabricNode& node : nodes)
    {
        if (node.key == key)
            return graph.GetNode(node.id);
    }
    return nullptr;
}

JSONValue WorldFabricGraphResource::ToJSON() const
{
    JSONValue root(JSON_OBJECT);
    root.Set("version", 1u);

    JSONValue nodes(JSON_ARRAY);
    const ea::vector<WorldFabricNode> sortedNodes = graph_.GetNodes();
    for (const WorldFabricNode& node : sortedNodes)
    {
        JSONValue nodeJson(JSON_OBJECT);
        nodeJson.Set("key", node.key);
        nodeJson.Set("kind", GetNodeKindName(node.kind));
        nodeJson.Set("type", node.type);

        JSONValue metadata(JSON_OBJECT);
        metadata.SetStringVariantMap(node.metadata, context_);
        nodeJson.Set("metadata", ea::move(metadata));
        nodes.Push(ea::move(nodeJson));
    }
    root.Set("nodes", ea::move(nodes));

    JSONValue edges(JSON_ARRAY);
    for (const WorldFabricNode& node : sortedNodes)
    {
        const ea::vector<WorldFabricDependency> dependencies = graph_.GetDependencies(node.id);
        for (const WorldFabricDependency& dependency : dependencies)
        {
            const WorldFabricNode* dependencyNode = graph_.GetNode(dependency.dependency);
            if (!dependencyNode)
                continue;

            JSONValue edge(JSON_OBJECT);
            edge.Set("node", node.key);
            edge.Set("dependency", dependencyNode->key);
            edge.Set("kind", GetDependencyKindName(dependency.kind));
            edge.Set("label", dependency.label);
            edges.Push(ea::move(edge));
        }
    }
    root.Set("edges", ea::move(edges));
    return root;
}

bool WorldFabricGraphResource::FromJSON(const JSONValue& root, ea::string* error)
{
    if (!root.IsObject() || !root.Get("version").IsNumber() || root.Get("version").GetUInt() != 1u
        || !root.Get("nodes").IsArray() || !root.Get("edges").IsArray())
    {
        if (error)
            *error = "World Fabric resource requires version 1, nodes and edges arrays";
        return false;
    }

    WorldFabricGraph candidate;
    for (const JSONValue& nodeJson : root.Get("nodes").GetArray())
    {
        if (!nodeJson.IsObject() || !nodeJson.Get("key").IsString() || !nodeJson.Get("kind").IsString()
            || !nodeJson.Get("type").IsString() || !nodeJson.Get("metadata").IsObject())
        {
            if (error)
                *error = "World Fabric contains an invalid node";
            return false;
        }

        const ea::string key = nodeJson.Get("key").GetString();
        if (key.empty() || FindNodeByKey(candidate, key))
        {
            if (error)
                *error = "World Fabric node keys must be non-empty and unique";
            return false;
        }

        WorldFabricNodeKind kind;
        if (!ParseNodeKind(nodeJson.Get("kind").GetString(), kind))
        {
            if (error)
                *error = Format("Unknown World Fabric node kind for '{}'", key);
            return false;
        }

        const WorldFabricId id = candidate.AddNode(key, kind, nodeJson.Get("type").GetString(),
            nodeJson.Get("metadata").GetStringVariantMap());
        if (id == InvalidWorldFabricId)
        {
            if (error)
                *error = Format("Unable to add World Fabric node '{}': {}", key, candidate.GetLastError());
            return false;
        }
    }

    for (const JSONValue& edgeJson : root.Get("edges").GetArray())
    {
        if (!edgeJson.IsObject() || !edgeJson.Get("node").IsString() || !edgeJson.Get("dependency").IsString()
            || !edgeJson.Get("kind").IsString() || !edgeJson.Get("label").IsString())
        {
            if (error)
                *error = "World Fabric contains an invalid dependency edge";
            return false;
        }

        const WorldFabricNode* node = FindNodeByKey(candidate, edgeJson.Get("node").GetString());
        const WorldFabricNode* dependency = FindNodeByKey(candidate, edgeJson.Get("dependency").GetString());
        WorldFabricDependencyKind kind;
        if (!node || !dependency || !ParseDependencyKind(edgeJson.Get("kind").GetString(), kind))
        {
            if (error)
                *error = "World Fabric dependency references an unknown node or kind";
            return false;
        }
        if (!candidate.AddDependency(node->id, dependency->id, kind, edgeJson.Get("label").GetString()))
        {
            if (error)
                *error = Format("Unable to add World Fabric dependency: {}", candidate.GetLastError());
            return false;
        }
    }

    ea::string validationError;
    if (!candidate.Validate(&validationError))
    {
        if (error)
            *error = validationError;
        return false;
    }

    graph_ = ea::move(candidate);
    return true;
}

bool WorldFabricGraphResource::Validate(ea::string* error) const
{
    return graph_.Validate(error);
}

ea::vector<WorldFabricId> WorldFabricGraphResource::GetBuildOrder(ea::string* error) const
{
    ea::vector<WorldFabricId> order;
    graph_.BuildOrder(order, error);
    return order;
}

unsigned WorldFabricGraphResource::SeedObjectReflection()
{
    return WorldFabricReflection::RegisterObjectReflection(context_, graph_);
}

void WorldFabricGraphResource::SerializeInBlock(Archive& archive)
{
    (void)archive;
}

bool WorldFabricGraphResource::BeginLoad(Deserializer& source)
{
    JSONFile jsonFile(context_);
    if (!jsonFile.Load(source))
        return false;
    return FromJSON(jsonFile.GetRoot());
}

bool WorldFabricGraphResource::Save(Serializer& dest) const
{
    JSONFile jsonFile(context_);
    jsonFile.GetRoot() = ToJSON();
    return jsonFile.Save(dest);
}

} // namespace Urho3D
