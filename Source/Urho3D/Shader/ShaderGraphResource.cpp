// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#include "../Precompiled.h"

#include "ShaderGraphResource.h"

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

bool IsValidNodeKind(unsigned value)
{
    return value <= static_cast<unsigned>(ShaderGraphNodeKind::Output);
}

bool IsValidValueType(unsigned value)
{
    return value <= static_cast<unsigned>(ShaderGraphValueType::Texture2D);
}

} // namespace

ShaderGraphResource::ShaderGraphResource(Context* context)
    : Resource(context)
{
}

bool ShaderGraphResource::CheckExtension(const ea::string& fileName)
{
    return fileName.ends_with(".shadergraph", false);
}

JSONValue ShaderGraphResource::ToJSON() const
{
    JSONValue root(JSON_OBJECT);
    root.Set("version", 1u);
    root.Set("output", graph_.GetOutputNode() ? graph_.GetOutputNode()->id : 0u);

    JSONValue parameters(JSON_ARRAY);
    for (const ShaderGraphParameter& parameter : graph_.GetParameters())
    {
        JSONValue item(JSON_OBJECT);
        item.Set("name", parameter.name);
        item.Set("type", static_cast<unsigned>(parameter.type));
        item.Set("default", MakeVariantJSON(parameter.defaultValue, context_));
        parameters.Push(ea::move(item));
    }
    root.Set("parameters", ea::move(parameters));

    JSONValue nodes(JSON_ARRAY);
    for (const ShaderGraphNode& node : graph_.GetNodes())
    {
        JSONValue item(JSON_OBJECT);
        item.Set("id", node.id);
        item.Set("name", node.name);
        item.Set("kind", static_cast<unsigned>(node.kind));
        item.Set("type", static_cast<unsigned>(node.valueType));
        item.Set("value", MakeVariantJSON(node.value, context_));
        nodes.Push(ea::move(item));
    }
    root.Set("nodes", ea::move(nodes));

    JSONValue connections(JSON_ARRAY);
    for (const ShaderGraphConnection& connection : graph_.GetConnections())
    {
        JSONValue item(JSON_OBJECT);
        item.Set("from", connection.fromNode);
        item.Set("fromPin", connection.fromPin);
        item.Set("to", connection.toNode);
        item.Set("toPin", connection.toPin);
        connections.Push(ea::move(item));
    }
    root.Set("connections", ea::move(connections));
    return root;
}

bool ShaderGraphResource::FromJSON(const JSONValue& root, ea::string* error)
{
    if (!root.IsObject())
    {
        if (error)
            *error = "ShaderGraph root must be a JSON object";
        return false;
    }

    const JSONValue& nodes = root.Get("nodes");
    const JSONValue& connections = root.Get("connections");
    const JSONValue& parameters = root.Get("parameters");
    if (!nodes.IsArray() || !connections.IsArray() || !parameters.IsArray())
    {
        if (error)
            *error = "ShaderGraph requires nodes, connections and parameters arrays";
        return false;
    }

    ShaderGraph candidate;
    for (const JSONValue& item : parameters.GetArray())
    {
        if (!item.IsObject() || !item.Get("name").IsString() || !item.Get("type").IsNumber()
            || !IsValidValueType(item.Get("type").GetUInt()))
        {
            if (error)
                *error = "ShaderGraph contains an invalid parameter";
            return false;
        }

        ShaderGraphParameter parameter;
        parameter.name = item.Get("name").GetString();
        parameter.type = static_cast<ShaderGraphValueType>(item.Get("type").GetUInt());
        parameter.defaultValue = ReadVariantJSON(item.Get("default"));
        if (!candidate.SetParameter(parameter))
        {
            if (error)
                *error = Format("Duplicate or invalid ShaderGraph parameter: {}", parameter.name);
            return false;
        }
    }

    ea::unordered_map<unsigned, unsigned> idMap;
    for (const JSONValue& item : nodes.GetArray())
    {
        if (!item.IsObject() || !item.Get("id").IsNumber() || !item.Get("name").IsString()
            || !item.Get("kind").IsNumber() || !item.Get("type").IsNumber()
            || !IsValidNodeKind(item.Get("kind").GetUInt()) || !IsValidValueType(item.Get("type").GetUInt()))
        {
            if (error)
                *error = "ShaderGraph contains an invalid node";
            return false;
        }

        const unsigned sourceId = item.Get("id").GetUInt();
        if (!sourceId || idMap.contains(sourceId))
        {
            if (error)
                *error = "ShaderGraph contains a duplicate or zero node identifier";
            return false;
        }

        const unsigned newId = candidate.AddNode(item.Get("name").GetString(),
            static_cast<ShaderGraphNodeKind>(item.Get("kind").GetUInt()),
            static_cast<ShaderGraphValueType>(item.Get("type").GetUInt()), ReadVariantJSON(item.Get("value")));
        idMap[sourceId] = newId;
    }

    for (const JSONValue& item : connections.GetArray())
    {
        if (!item.IsObject() || !item.Get("from").IsNumber() || !item.Get("to").IsNumber()
            || !item.Get("fromPin").IsString() || !item.Get("toPin").IsString())
        {
            if (error)
                *error = "ShaderGraph contains an invalid connection";
            return false;
        }

        const auto from = idMap.find(item.Get("from").GetUInt());
        const auto to = idMap.find(item.Get("to").GetUInt());
        if (from == idMap.end() || to == idMap.end()
            || !candidate.Connect(from->second, item.Get("fromPin").GetString(), to->second, item.Get("toPin").GetString()))
        {
            if (error)
                *error = "ShaderGraph contains an invalid or duplicate connection";
            return false;
        }
    }

    const unsigned sourceOutput = root.Get("output").GetUInt();
    if (sourceOutput)
    {
        const auto output = idMap.find(sourceOutput);
        if (output == idMap.end() || !candidate.SetOutputNode(output->second))
        {
            if (error)
                *error = "ShaderGraph output node does not exist or is not an output node";
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

void ShaderGraphResource::SerializeInBlock(Archive& archive)
{
    // The resource uses JSONValue directly to keep the file format explicit and stable.
    // Archive serialization remains intentionally empty for generic reflection callers.
    (void)archive;
}

bool ShaderGraphResource::BeginLoad(Deserializer& source)
{
    JSONFile jsonFile(context_);
    if (!jsonFile.Load(source))
        return false;
    return FromJSON(jsonFile.GetRoot());
}

bool ShaderGraphResource::Save(Serializer& dest) const
{
    JSONFile jsonFile(context_);
    jsonFile.GetRoot() = ToJSON();
    return jsonFile.Save(dest);
}

} // namespace Urho3D
