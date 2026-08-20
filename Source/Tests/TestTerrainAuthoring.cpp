// SPDX-License-Identifier: MIT

#include <catch2/catch_amalgamated.hpp>

#include <Urho3D/WorldFabric/TerrainAuthoring.h>

using namespace Urho3D;

TEST_CASE("Terrain authoring falloffs are deterministic", "[terrain][authoring]")
{
    REQUIRE(TerrainAuthoring::EvaluateFalloff(TerrainBrushFalloff::Constant, 0.5f) == Catch::Approx(1.0f));
    REQUIRE(TerrainAuthoring::EvaluateFalloff(TerrainBrushFalloff::Linear, 0.0f) == Catch::Approx(1.0f));
    REQUIRE(TerrainAuthoring::EvaluateFalloff(TerrainBrushFalloff::Linear, 1.0f) == Catch::Approx(0.0f));
    REQUIRE(TerrainAuthoring::EvaluateFalloff(TerrainBrushFalloff::SmoothStep, 0.5f) == Catch::Approx(0.5f));
    REQUIRE(TerrainAuthoring::EvaluateFalloff(TerrainBrushFalloff::Linear, -1.0f) == Catch::Approx(1.0f));
}

TEST_CASE("Terrain authoring stamps and digests a height field", "[terrain][authoring]")
{
    std::vector<float> heights(9, 0.0f);
    TerrainBrushSettings settings;
    settings.radius = 1.5f;
    settings.strength = 0.5f;
    settings.falloff = TerrainBrushFalloff::Linear;

    std::string error;
    REQUIRE(TerrainAuthoring::ApplyHeightStamp(heights, 3, 3, 1.0f, 1.0f, settings, &error));
    REQUIRE(error.empty());
    REQUIRE(heights[4] == Catch::Approx(0.5f));
    REQUIRE(heights[0] == Catch::Approx(0.0f));

    const unsigned long long firstDigest = TerrainAuthoring::ComputeDigest(heights, 3, 3);
    const unsigned long long secondDigest = TerrainAuthoring::ComputeDigest(heights, 3, 3);
    REQUIRE(firstDigest == secondDigest);
    REQUIRE(TerrainAuthoring::QuantizeHeight(0.0f) == 0u);
    REQUIRE(TerrainAuthoring::QuantizeHeight(1.0f) == 65535u);
}

TEST_CASE("Terrain authoring rejects unsafe settings and dimensions", "[terrain][authoring]")
{
    TerrainBrushSettings settings;
    settings.radius = 0.0f;
    std::string error;
    std::vector<float> validHeights(4);
    REQUIRE_FALSE(TerrainAuthoring::ApplyHeightStamp(validHeights, 2, 2, 0.5f, 0.5f, settings, &error));
    REQUIRE_FALSE(error.empty());

    settings.radius = 1.0f;
    error.clear();
    std::vector<float> malformedHeights(3);
    REQUIRE_FALSE(TerrainAuthoring::ApplyHeightStamp(malformedHeights, 2, 2, 0.5f, 0.5f, settings, &error));
    REQUIRE_FALSE(error.empty());
}
