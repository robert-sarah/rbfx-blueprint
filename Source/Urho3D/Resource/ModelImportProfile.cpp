// SPDX-License-Identifier: MIT

#include "ModelImportProfile.h"

#include <algorithm>
#include <cmath>

namespace Urho3D
{

namespace
{

void SetError(std::string* error, const char* message)
{
    if (error)
        *error = message;
}

std::string NormalizeFormat(const std::string& value)
{
    std::string normalized = value;
    for (char& character : normalized)
    {
        if (character >= 'A' && character <= 'Z')
            character = static_cast<char>(character - 'A' + 'a');
    }
    return normalized;
}

bool IsSupportedFormat(const std::string& format)
{
    return format == "gltf" || format == "glb" || format == "obj" || format == "fbx" || format == "dae";
}

} // namespace

bool ModelImportProfile::Validate(std::string* error) const
{
    if (version != 1)
    {
        SetError(error, "Model import profile version must be 1.");
        return false;
    }

    const std::string normalizedFormat = NormalizeFormat(sourceFormat);
    if (!IsSupportedFormat(normalizedFormat))
    {
        SetError(error, "Model import profile has an unsupported source format.");
        return false;
    }
    if (!std::isfinite(unitsPerMeter) || unitsPerMeter <= 0.0 || unitsPerMeter > 1000.0)
    {
        SetError(error, "Model import profile unitsPerMeter must be finite and within (0, 1000].");
        return false;
    }
    if (uvChannelCount == 0 || uvChannelCount > 8)
    {
        SetError(error, "Model import profile uvChannelCount must be between 1 and 8.");
        return false;
    }
    if (lodScreenSizes.empty() || lodScreenSizes.size() > 16)
    {
        SetError(error, "Model import profile requires between 1 and 16 LOD screen-size thresholds.");
        return false;
    }

    float previous = 1.000001f;
    for (const float threshold : lodScreenSizes)
    {
        if (!std::isfinite(threshold) || threshold <= 0.0f || threshold > 1.0f || threshold >= previous)
        {
            SetError(error, "Model import profile LOD thresholds must be finite, descending and within (0, 1].");
            return false;
        }
        previous = threshold;
    }
    if (provenance.empty() || provenance.size() > 256)
    {
        SetError(error, "Model import profile provenance must contain between 1 and 256 characters.");
        return false;
    }
    return true;
}

JSONValue ModelImportProfile::ToJSON() const
{
    JSONValue root(JSON_OBJECT);
    root.Set("version", version);
    root.Set("sourceFormat", ea::string(NormalizeFormat(sourceFormat).c_str()));
    root.Set("upAxis", static_cast<unsigned>(upAxis));
    root.Set("handedness", static_cast<unsigned>(handedness));
    root.Set("unitsPerMeter", unitsPerMeter);
    root.Set("generateTangents", generateTangents);
    root.Set("generateLightmapUV", generateLightmapUV);
    root.Set("uvChannelCount", uvChannelCount);

    JSONValue lods(JSON_ARRAY);
    for (const float threshold : lodScreenSizes)
        lods.Push(threshold);
    root.Set("lodScreenSizes", ea::move(lods));
    root.Set("provenance", ea::string(provenance.c_str()));
    return root;
}

bool ModelImportProfile::FromJSON(const JSONValue& value, std::string* error)
{
    if (!value.IsObject())
    {
        SetError(error, "Model import profile root must be an object.");
        return false;
    }

    const char* required[] = {"version", "sourceFormat", "upAxis", "handedness", "unitsPerMeter",
        "generateTangents", "generateLightmapUV", "uvChannelCount", "lodScreenSizes", "provenance"};
    for (const char* field : required)
    {
        if (!value.Contains(field))
        {
            if (error)
                *error = std::string("Model import profile is missing field '") + field + "'.";
            return false;
        }
    }
    if (!value["lodScreenSizes"].IsArray())
    {
        SetError(error, "Model import profile lodScreenSizes must be an array.");
        return false;
    }

    ModelImportProfile parsed;
    parsed.version = value["version"].GetUInt();
    parsed.sourceFormat = value["sourceFormat"].GetString().c_str();
    parsed.upAxis = static_cast<ModelUpAxis>(value["upAxis"].GetUInt());
    parsed.handedness = static_cast<ModelHandedness>(value["handedness"].GetUInt());
    parsed.unitsPerMeter = value["unitsPerMeter"].GetDouble();
    parsed.generateTangents = value["generateTangents"].GetBool();
    parsed.generateLightmapUV = value["generateLightmapUV"].GetBool();
    parsed.uvChannelCount = value["uvChannelCount"].GetUInt();
    parsed.provenance = value["provenance"].GetString().c_str();
    parsed.lodScreenSizes.clear();
    for (const JSONValue& item : value["lodScreenSizes"].GetArray())
        parsed.lodScreenSizes.push_back(item.GetFloat());

    if (static_cast<unsigned>(parsed.upAxis) > static_cast<unsigned>(ModelUpAxis::Z)
        || static_cast<unsigned>(parsed.handedness) > static_cast<unsigned>(ModelHandedness::Right))
    {
        SetError(error, "Model import profile contains an invalid axis or handedness value.");
        return false;
    }
    if (!parsed.Validate(error))
        return false;
    *this = std::move(parsed);
    return true;
}

unsigned ModelImportProfile::CalculateHash() const
{
    unsigned result = 0;
    CombineHash(result, version);
    CombineHash(result, MakeHash(ea::string(NormalizeFormat(sourceFormat).c_str())));
    CombineHash(result, static_cast<unsigned>(upAxis));
    CombineHash(result, static_cast<unsigned>(handedness));
    CombineHash(result, static_cast<unsigned>(std::llround(unitsPerMeter * 1000000.0)));
    CombineHash(result, generateTangents ? 1u : 0u);
    CombineHash(result, generateLightmapUV ? 1u : 0u);
    CombineHash(result, uvChannelCount);
    for (const float threshold : lodScreenSizes)
        CombineHash(result, static_cast<unsigned>(std::llround(threshold * 1000000.0f)));
    CombineHash(result, MakeHash(ea::string(provenance.c_str())));
    return result;
}

} // namespace Urho3D
