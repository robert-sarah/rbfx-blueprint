// SPDX-License-Identifier: MIT

#include <Urho3D/Resource/ModelImportProfile.h>

#include "CommonUtils.h"

using namespace Urho3D;

TEST_CASE("Model import profile round-trips deterministic settings", "[assets][model-import]")
{
    ModelImportProfile profile;
    profile.sourceFormat = "FBX";
    profile.upAxis = ModelUpAxis::Z;
    profile.handedness = ModelHandedness::Left;
    profile.unitsPerMeter = 0.01;
    profile.generateTangents = false;
    profile.generateLightmapUV = true;
    profile.uvChannelCount = 2;
    profile.lodScreenSizes = {0.8f, 0.4f, 0.1f};
    profile.provenance = "artist-asset:character.fbx";

    std::string error;
    REQUIRE(profile.Validate(&error));
    REQUIRE(error.empty());
    const unsigned digest = profile.CalculateHash();

    const JSONValue serialized = profile.ToJSON();
    CHECK(serialized["sourceFormat"].GetString() == "fbx");
    CHECK(serialized["lodScreenSizes"].GetArray().size() == 3);

    ModelImportProfile restored;
    REQUIRE(restored.FromJSON(serialized, &error));
    CHECK(restored.upAxis == ModelUpAxis::Z);
    CHECK(restored.handedness == ModelHandedness::Left);
    CHECK(restored.unitsPerMeter == Catch::Approx(0.01));
    CHECK(restored.CalculateHash() == digest);
}

TEST_CASE("Model import profile rejects unsafe production values", "[assets][model-import]")
{
    ModelImportProfile profile;
    std::string error;

    profile.sourceFormat = "blend";
    CHECK_FALSE(profile.Validate(&error));
    CHECK_FALSE(error.empty());

    profile = ModelImportProfile{};
    profile.unitsPerMeter = 0.0;
    CHECK_FALSE(profile.Validate(&error));

    profile = ModelImportProfile{};
    profile.uvChannelCount = 9;
    CHECK_FALSE(profile.Validate(&error));

    profile = ModelImportProfile{};
    profile.lodScreenSizes = {0.25f, 0.5f};
    CHECK_FALSE(profile.Validate(&error));

    profile = ModelImportProfile{};
    profile.provenance.clear();
    CHECK_FALSE(profile.Validate(&error));
}

TEST_CASE("Model import profile requires a complete JSON schema", "[assets][model-import]")
{
    ModelImportProfile profile;
    JSONValue incomplete(JSON_OBJECT);
    incomplete.Set("version", 1u);
    std::string error;

    CHECK_FALSE(profile.FromJSON(incomplete, &error));
    CHECK(error.find("sourceFormat") != std::string::npos);

    ModelImportProfile a;
    ModelImportProfile b;
    a.sourceFormat = "GLTF";
    b.sourceFormat = "gltf";
    CHECK(a.CalculateHash() == b.CalculateHash());

    b.provenance = "different-source";
    CHECK(a.CalculateHash() != b.CalculateHash());
}
