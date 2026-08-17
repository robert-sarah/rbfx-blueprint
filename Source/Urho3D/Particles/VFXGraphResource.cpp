// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#include "../Precompiled.h"

#include "VFXGraphResource.h"

#include "../IO/Deserializer.h"
#include "../IO/Serializer.h"
#include "../Resource/JSONFile.h"

#include <EASTL/unordered_map.h>

namespace Urho3D
{

namespace
{

JSONValue MakeVariantJSON(const Variant& value, Context* context)
{
    JSONValue json;
    json.SetVariant(value, context);
    return json;
}

Variant ReadVariantJSON(const JSONValue& value)
{
    return value.IsNull() ? Variant{} : value.GetVariant();
}

bool IsValidNodeType(unsigned value)
{
    return value <= static_cast<unsigned>(VFXNodeType::Output);
}

bool IsValidSimulationMode(unsigned value)
{
    return value <= static_cast<unsigned>(VFXSimulationMode::GPU);
}

} // namespace

VFXGraphResource::VFXGraphResource(Context* context)
    : Resource(context)
{
}

bool VFXGraphResource::CheckExtension(const ea::string& fileName)
{
    return fileName.ends_with(".vfxgraph", false);
}

JSONValue VFXGraphResource::ToJSON() const
{
    JSONValue root(JSON_OBJECT);
    root.Set("version", 1u);
    root.Set("output", graph_.GetOutputNodeId());
    root.Set("simulationMode", static_cast<unsigned>(graph_.GetSimulationMode()));
    root.Set("maxParticles", graph_.GetMaxParticles());
    root.Set("spawnRate", graph_.GetSpawnRate());
    root.Set("particleLifetime", graph_.GetParticleLifetime());
    root.Set("initialVelocity", MakeVariantJSON(Variant(graph_.GetInitialVelocity()), context_));
    root.Set("force", MakeVariantJSON(Variant(graph_.GetForce()), context_));
    root.Set("drag", graph_.GetDrag());
    root.Set("ribbonTrailLength", graph_.GetRibbonTrailLength());

    JSONValue nodes(JSON_ARRAY);
    for (const VFXNode& node : graph_.GetNodes())
    {
        if (!node.id)
            continue;
        JSONValue item(JSON_OBJECT);
        item.Set("id", node.id);
        item.Set("name", node.name);
        item.Set("type", static_cast<unsigned>(node.type));
        item.Set("vector", MakeVariantJSON(Variant(node.vectorValue), context_));
        item.Set("scalar", node.scalarValue);
        nodes.Push(ea::move(item));
    }
    root.Set("nodes", ea::move(nodes));
    return root;
}

bool VFXGraphResource::FromJSON(const JSONValue& root, ea::string* error)
{
    if (!root.IsObject())
    {
        if (error)
            *error = "VFXGraph root must be a JSON object";
        return false;
    }

    const JSONValue& nodes = root.Get("nodes");
    if (!nodes.IsArray() || !root.Get("simulationMode").IsNumber() || !root.Get("maxParticles").IsNumber()
        || !root.Get("spawnRate").IsNumber() || !root.Get("particleLifetime").IsNumber()
        || !root.Get("initialVelocity").IsObject() || !root.Get("force").IsObject()
        || !root.Get("drag").IsNumber() || !root.Get("ribbonTrailLength").IsNumber())
    {
        if (error)
            *error = "VFXGraph requires simulation settings and a nodes array";
        return false;
    }

    const unsigned simulationMode = root.Get("simulationMode").GetUInt();
    if (!IsValidSimulationMode(simulationMode) || root.Get("maxParticles").GetUInt() == 0)
    {
        if (error)
            *error = "VFXGraph contains invalid simulation settings";
        return false;
    }

    VFXGraph candidate;
    candidate.SetSimulationMode(static_cast<VFXSimulationMode>(simulationMode));
    candidate.SetMaxParticles(root.Get("maxParticles").GetUInt());
    candidate.SetSpawnRate(root.Get("spawnRate").GetFloat());
    candidate.SetParticleLifetime(root.Get("particleLifetime").GetFloat());
    candidate.SetInitialVelocity(ReadVariantJSON(root.Get("initialVelocity")).GetVector3());
    candidate.SetForce(ReadVariantJSON(root.Get("force")).GetVector3());
    candidate.SetDrag(root.Get("drag").GetFloat());
    candidate.SetRibbonTrailLength(root.Get("ribbonTrailLength").GetUInt());

    ea::unordered_map<unsigned, unsigned> idMap;
    for (const JSONValue& item : nodes.GetArray())
    {
        if (!item.IsObject() || !item.Get("id").IsNumber() || !item.Get("name").IsString()
            || !item.Get("type").IsNumber() || !item.Get("vector").IsObject() || !item.Get("scalar").IsNumber()
            || !IsValidNodeType(item.Get("type").GetUInt()))
        {
            if (error)
                *error = "VFXGraph contains an invalid node";
            return false;
        }

        const unsigned sourceId = item.Get("id").GetUInt();
        if (!sourceId || idMap.contains(sourceId))
        {
            if (error)
                *error = "VFXGraph contains a duplicate or zero node identifier";
            return false;
        }

        const unsigned newId = candidate.AddNode(item.Get("name").GetString(),
            static_cast<VFXNodeType>(item.Get("type").GetUInt()));
        if (!newId)
        {
            if (error)
                *error = "VFXGraph contains an empty node name";
            return false;
        }
        VFXNode* node = candidate.GetNode(newId);
        node->vectorValue = ReadVariantJSON(item.Get("vector")).GetVector3();
        node->scalarValue = item.Get("scalar").GetFloat();
        idMap[sourceId] = newId;
    }

    const unsigned sourceOutput = root.Get("output").GetUInt();
    if (sourceOutput)
    {
        const auto output = idMap.find(sourceOutput);
        if (output == idMap.end() || !candidate.SetOutputNode(output->second))
        {
            if (error)
                *error = "VFXGraph output node does not exist or is not an Output node";
            return false;
        }
    }

    ea::string validationError;
    if (!candidate.Compile(&validationError))
    {
        if (error)
            *error = validationError;
        return false;
    }

    graph_ = ea::move(candidate);
    return true;
}

void VFXGraphResource::SerializeInBlock(Archive& archive)
{
    (void)archive;
}

bool VFXGraphResource::BeginLoad(Deserializer& source)
{
    JSONFile jsonFile(context_);
    if (!jsonFile.Load(source))
        return false;
    return FromJSON(jsonFile.GetRoot());
}

bool VFXGraphResource::Save(Serializer& dest) const
{
    JSONFile jsonFile(context_);
    jsonFile.GetRoot() = ToJSON();
    return jsonFile.Save(dest);
}

} // namespace Urho3D
