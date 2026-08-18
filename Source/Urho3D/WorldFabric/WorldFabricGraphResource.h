#pragma once

#include <Urho3D/Resource/JSONValue.h>
#include <Urho3D/Resource/Resource.h>
#include <Urho3D/WorldFabric/WorldFabric.h>

namespace Urho3D
{

/// Editable, JSON-backed semantic dependency graph shared by production systems.
class URHO3D_API WorldFabricGraphResource : public Resource
{
    URHO3D_OBJECT(WorldFabricGraphResource, Resource);

public:
    explicit WorldFabricGraphResource(Context* context);

    static bool CheckExtension(const ea::string& fileName);
    static const char* GetNodeKindName(WorldFabricNodeKind kind);
    static bool ParseNodeKind(const ea::string& name, WorldFabricNodeKind& kind);
    static const char* GetDependencyKindName(WorldFabricDependencyKind kind);
    static bool ParseDependencyKind(const ea::string& name, WorldFabricDependencyKind& kind);

    WorldFabricGraph& GetGraph() { return graph_; }
    const WorldFabricGraph& GetGraph() const { return graph_; }
    void Reset() { graph_.Reset(); }

    /// Register all reflected rbfx object types into this graph.
    unsigned SeedObjectReflection();

    JSONValue ToJSON() const;
    bool FromJSON(const JSONValue& root, ea::string* error = nullptr);
    bool Validate(ea::string* error = nullptr) const;
    ea::vector<WorldFabricId> GetBuildOrder(ea::string* error = nullptr) const;
    unsigned long long ComputeDigest() const { return graph_.ComputeDigest(); }

    void SerializeInBlock(Archive& archive) override;
    bool BeginLoad(Deserializer& source) override;
    bool Save(Serializer& dest) const override;

private:
    const WorldFabricNode* FindNodeByKey(const WorldFabricGraph& graph, const ea::string& key) const;

    WorldFabricGraph graph_;
};

} // namespace Urho3D
