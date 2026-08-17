// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <Urho3D/Resource/JSONValue.h>
#include <Urho3D/Resource/Resource.h>
#include <Urho3D/Shader/ShaderGraph.h>

namespace Urho3D
{

/// Loadable and editable project resource for a backend-neutral material shader graph.
class URHO3D_API ShaderGraphResource : public Resource
{
    URHO3D_OBJECT(ShaderGraphResource, Resource);

public:
    explicit ShaderGraphResource(Context* context);

    /// Return whether the file name uses the shader graph resource extension.
    static bool CheckExtension(const ea::string& fileName);

    /// Return the graph represented by this resource.
    ShaderGraph& GetGraph() { return graph_; }
    const ShaderGraph& GetGraph() const { return graph_; }

    /// Replace the graph and keep the resource data model in sync.
    void SetGraph(const ShaderGraph& graph) { graph_ = graph; }

    /// Convert the graph to/from the stable JSON resource representation.
    JSONValue ToJSON() const;
    bool FromJSON(const JSONValue& root, ea::string* error = nullptr);

    /// Implement Resource.
    /// @{
    void SerializeInBlock(Archive& archive) override;
    bool BeginLoad(Deserializer& source) override;
    bool Save(Serializer& dest) const override;
    /// @}

private:
    ShaderGraph graph_;
};

} // namespace Urho3D
