#include <Urho3D/IO/VectorBuffer.h>
#include <Urho3D/Network/MultiplayerProfileResource.h>

#include "CommonUtils.h"

#include <catch2/catch_amalgamated.hpp>

using namespace Urho3D;

TEST_CASE("Multiplayer profile resource round-trips and validates production settings", "[network][resource]")
{
    const auto context = Tests::GetOrCreateContext(Tests::CreateCompleteContext);
    MultiplayerProfileResource source(context.Get());
    source.SetMode(MultiplayerProfileResource::Mode::Server);
    source.SetAddress("0.0.0.0");
    source.SetPort(7777);
    source.SetMaxConnections(64);
    source.SetUpdateFps(60);
    source.SetPingIntervalMs(100);
    source.SetMaxPingIntervalMs(5000);
    source.SetClockBufferSize(32);
    source.SetPingBufferSize(8);
    source.SetPackageCacheDir("Cache/NetworkPackages");
    source.GetReplicationSettings()["InterpolationLimit"] = 0.5f;
    source.GetReplicationSettings()["MaxInputFrames"] = 512u;
    source.GetReplicationSettings()["MinTimeDilation"] = 0.8f;
    source.GetReplicationSettings()["MaxTimeDilation"] = 1.25f;

    ea::string error;
    REQUIRE(source.Validate(&error));
    CHECK(error.empty());
    const unsigned long long digest = source.ComputeDigest(&error);
    REQUIRE(digest != 0);
    CHECK(error.empty());

    const JSONValue serialized = source.ToJSON();
    MultiplayerProfileResource restored(context.Get());
    REQUIRE(restored.FromJSON(serialized, &error));
    CHECK(error.empty());
    CHECK(restored.GetMode() == MultiplayerProfileResource::Mode::Server);
    CHECK(restored.GetAddress() == "0.0.0.0");
    CHECK(restored.GetPort() == 7777);
    CHECK(restored.GetMaxConnections() == 64);
    CHECK(restored.GetUpdateFps() == 60);
    CHECK(restored.GetReplicationSettings().at("InterpolationLimit").GetFloat() == 0.5f);
    CHECK(restored.GetReplicationSettings().at("MaxInputFrames").GetUInt() == 512u);
    CHECK(restored.ComputeDigest(&error) == digest);

    VectorBuffer buffer;
    buffer.SetName("Production.multiplayer");
    REQUIRE(source.Save(buffer));
    REQUIRE(buffer.GetSize() > 0);
    buffer.Seek(0);
    MultiplayerProfileResource loaded(context.Get());
    REQUIRE(loaded.BeginLoad(buffer));
    CHECK(loaded.GetPort() == 7777);
    CHECK(loaded.GetMode() == MultiplayerProfileResource::Mode::Server);

    CHECK(MultiplayerProfileResource::CheckExtension("Dedicated.multiplayer"));
    CHECK(MultiplayerProfileResource::CheckExtension("Competitive.networkprofile"));
    CHECK_FALSE(MultiplayerProfileResource::CheckExtension("Competitive.json"));
    CHECK_FALSE(MultiplayerProfileResource::CheckExtension("Competitive.multiplayer.bak"));

    JSONValue missingSettings = serialized;
    missingSettings.Erase("replicationSettings");
    REQUIRE_FALSE(restored.FromJSON(missingSettings, &error));
    CHECK_FALSE(error.empty());

    JSONValue invalidPort = serialized;
    invalidPort["port"] = 0u;
    REQUIRE_FALSE(restored.FromJSON(invalidPort, &error));
    CHECK_FALSE(error.empty());

    JSONValue unsupportedSetting = serialized;
    unsupportedSetting["replicationSettings"]["UnknownSetting"] = 1u;
    REQUIRE_FALSE(restored.FromJSON(unsupportedSetting, &error));
    CHECK_FALSE(error.empty());

    JSONValue invalidDilation = serialized;
    invalidDilation["replicationSettings"]["MinTimeDilation"] = 2.0f;
    invalidDilation["replicationSettings"]["MaxTimeDilation"] = 1.0f;
    CHECK(invalidDilation["replicationSettings"]["MinTimeDilation"].GetFloat() == 2.0f);
    CHECK(invalidDilation["replicationSettings"]["MaxTimeDilation"].GetFloat() == 1.0f);
    REQUIRE_FALSE(restored.FromJSON(invalidDilation, &error));
    CHECK_FALSE(error.empty());
}
