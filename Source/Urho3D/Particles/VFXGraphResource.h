// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <Urho3D/Particles/VFXGraph.h>
#include <Urho3D/Resource/JSONValue.h>
#include <Urho3D/Resource/Resource.h>

namespace Urho3D
{

/// Loadable and editable production resource for a deterministic VFX graph.
class URHO3D_API VFXGraphResource : public Resource
{
    URHO3D_OBJECT(VFXGraphResource, Resource);

public:
    explicit VFXGraphResource(Context* context);

    static bool CheckExtension(const ea::string& fileName);

    VFXGraph& GetGraph() { return graph_; }
    const VFXGraph& GetGraph() const { return graph_; }
    void SetGraph(const VFXGraph& graph) { graph_ = graph; }

    JSONValue ToJSON() const;
    bool FromJSON(const JSONValue& root, ea::string* error = nullptr);

    void SerializeInBlock(Archive& archive) override;
    bool BeginLoad(Deserializer& source) override;
    bool Save(Serializer& dest) const override;

private:
    VFXGraph graph_;
};

} // namespace Urho3D
