#include "../Precompiled.h"

#include "MultiplayerProfileResource.h"

#include "Network.h"
#include "../IO/Deserializer.h"
#include "../IO/Serializer.h"
#include "../IO/VectorBuffer.h"
#include "../Resource/JSONFile.h"

#include <cmath>

namespace Urho3D
{

namespace
{

const char* const SupportedSettings[] = {
    "InterpolationLimit",
    "MaxInputFrames",
    "MaxInputRedundancy",
    "PeriodicClockInterval",
    "InputDelayFilterBufferSize",
    "InputBufferingFilterBufferSize",
    "InputBufferingWindowSize",
    "InputBufferingTweakA",
    "InputBufferingTweakB",
    "InputBufferingMin",
    "InputBufferingMax",
    "RelevanceTimeout",
    "ServerTracingDuration",
    "TimeErrorTolerance",
    "TimeSnapThreshold",
    "MinTimeDilation",
    "MaxTimeDilation",
    "InterpolationDelay",
    "ClientTracingDuration",
    "ExtrapolationLimit",
};

unsigned long long HashBytes(const unsigned char* data, unsigned size)
{
    unsigned long long hash = 1469598103934665603ull;
    for (unsigned index = 0; index < size; ++index)
    {
        hash ^= data[index];
        hash *= 1099511628211ull;
    }
    return hash;
}

} // namespace

MultiplayerProfileResource::MultiplayerProfileResource(Context* context)
    : Resource(context)
{
    replicationSettings_["InterpolationLimit"] = 0.25f;
    replicationSettings_["MaxInputFrames"] = 256u;
    replicationSettings_["MaxInputRedundancy"] = 32u;
    replicationSettings_["InterpolationDelay"] = 0.1f;
    replicationSettings_["MinTimeDilation"] = 0.7f;
    replicationSettings_["MaxTimeDilation"] = 1.5f;
}

bool MultiplayerProfileResource::CheckExtension(const ea::string& fileName)
{
    return fileName.ends_with(".multiplayer", false) || fileName.ends_with(".networkprofile", false);
}

const char* MultiplayerProfileResource::GetModeName(Mode mode)
{
    switch (mode)
    {
    case Mode::Server: return "Server";
    case Mode::Client: return "Client";
    case Mode::ListenServer: return "ListenServer";
    default: return "ListenServer";
    }
}

bool MultiplayerProfileResource::ParseMode(const ea::string& name, Mode& mode)
{
    if (name == "Server")
        mode = Mode::Server;
    else if (name == "Client")
        mode = Mode::Client;
    else if (name == "ListenServer")
        mode = Mode::ListenServer;
    else
        return false;
    return true;
}

bool MultiplayerProfileResource::IsSupportedReplicationSetting(const ea::string& name)
{
    for (const char* supported : SupportedSettings)
    {
        if (name == supported)
            return true;
    }
    return false;
}

bool MultiplayerProfileResource::IsSettingValueValid(const ea::string& name, const Variant& value)
{
    const VariantType type = value.GetType();
    if (type != VAR_INT && type != VAR_INT64 && type != VAR_FLOAT && type != VAR_DOUBLE)
        return false;

    const double numericValue = value.GetDouble();
    if (!std::isfinite(numericValue))
        return false;

    if (name == "InterpolationLimit" || name == "PeriodicClockInterval" || name == "RelevanceTimeout"
        || name == "ServerTracingDuration" || name == "TimeErrorTolerance" || name == "TimeSnapThreshold"
        || name == "MinTimeDilation" || name == "MaxTimeDilation" || name == "InterpolationDelay"
        || name == "ClientTracingDuration" || name == "ExtrapolationLimit" || name == "InputBufferingTweakA"
        || name == "InputBufferingTweakB")
    {
        return numericValue >= 0.0;
    }

    return numericValue > 0.0;
}

JSONValue MultiplayerProfileResource::ToJSON() const
{
    JSONValue root(JSON_OBJECT);
    root.Set("version", 1u);
    root.Set("mode", GetModeName(mode_));
    root.Set("address", address_);
    root.Set("port", port_);
    root.Set("maxConnections", maxConnections_);
    root.Set("updateFps", updateFps_);
    root.Set("pingIntervalMs", pingIntervalMs_);
    root.Set("maxPingIntervalMs", maxPingIntervalMs_);
    root.Set("clockBufferSize", clockBufferSize_);
    root.Set("pingBufferSize", pingBufferSize_);
    root.Set("packageCacheDir", packageCacheDir_);

    JSONValue settings(JSON_OBJECT);
    settings.SetStringVariantMap(replicationSettings_, context_);
    root.Set("replicationSettings", ea::move(settings));
    return root;
}

bool MultiplayerProfileResource::FromJSON(const JSONValue& root, ea::string* error)
{
    if (!root.IsObject())
    {
        if (error)
            *error = "Multiplayer profile root must be a JSON object";
        return false;
    }

    const JSONValue& version = root.Get("version");
    const JSONValue& mode = root.Get("mode");
    const JSONValue& address = root.Get("address");
    const JSONValue& port = root.Get("port");
    const JSONValue& maxConnections = root.Get("maxConnections");
    const JSONValue& updateFps = root.Get("updateFps");
    const JSONValue& pingIntervalMs = root.Get("pingIntervalMs");
    const JSONValue& maxPingIntervalMs = root.Get("maxPingIntervalMs");
    const JSONValue& clockBufferSize = root.Get("clockBufferSize");
    const JSONValue& pingBufferSize = root.Get("pingBufferSize");
    const JSONValue& packageCacheDir = root.Get("packageCacheDir");
    const JSONValue& settings = root.Get("replicationSettings");

    Mode parsedMode;
    if (!version.IsNumber() || version.GetUInt() != 1u || !mode.IsString() || !ParseMode(mode.GetString(), parsedMode)
        || !address.IsString() || !port.IsNumber() || !maxConnections.IsNumber() || !updateFps.IsNumber()
        || !pingIntervalMs.IsNumber() || !maxPingIntervalMs.IsNumber() || !clockBufferSize.IsNumber()
        || !pingBufferSize.IsNumber() || !packageCacheDir.IsString() || !settings.IsObject())
    {
        if (error)
            *error = "Multiplayer profile requires version 1, mode, address, port, limits and replicationSettings";
        return false;
    }

    MultiplayerProfileResource candidate(context_);
    candidate.mode_ = parsedMode;
    candidate.address_ = address.GetString();
    candidate.port_ = port.GetUInt();
    candidate.maxConnections_ = maxConnections.GetUInt();
    candidate.updateFps_ = updateFps.GetUInt();
    candidate.pingIntervalMs_ = pingIntervalMs.GetUInt();
    candidate.maxPingIntervalMs_ = maxPingIntervalMs.GetUInt();
    candidate.clockBufferSize_ = clockBufferSize.GetUInt();
    candidate.pingBufferSize_ = pingBufferSize.GetUInt();
    candidate.packageCacheDir_ = packageCacheDir.GetString();
    candidate.replicationSettings_ = settings.GetStringVariantMap();

    if (!candidate.Validate(error))
        return false;

    mode_ = candidate.mode_;
    address_ = ea::move(candidate.address_);
    port_ = candidate.port_;
    maxConnections_ = candidate.maxConnections_;
    updateFps_ = candidate.updateFps_;
    pingIntervalMs_ = candidate.pingIntervalMs_;
    maxPingIntervalMs_ = candidate.maxPingIntervalMs_;
    clockBufferSize_ = candidate.clockBufferSize_;
    pingBufferSize_ = candidate.pingBufferSize_;
    packageCacheDir_ = ea::move(candidate.packageCacheDir_);
    replicationSettings_ = ea::move(candidate.replicationSettings_);
    return true;
}

bool MultiplayerProfileResource::Validate(ea::string* error) const
{
    if (address_.empty())
    {
        if (error)
            *error = "Multiplayer profile address must not be empty";
        return false;
    }
    if (port_ == 0 || port_ > 65535)
    {
        if (error)
            *error = "Multiplayer profile port must be in range 1..65535";
        return false;
    }
    if (maxConnections_ == 0 || maxConnections_ > 4096)
    {
        if (error)
            *error = "Multiplayer profile maxConnections must be in range 1..4096";
        return false;
    }
    if (updateFps_ == 0 || updateFps_ > 240)
    {
        if (error)
            *error = "Multiplayer profile updateFps must be in range 1..240";
        return false;
    }
    if (pingIntervalMs_ == 0 || maxPingIntervalMs_ < pingIntervalMs_ || clockBufferSize_ == 0 || pingBufferSize_ == 0)
    {
        if (error)
            *error = "Multiplayer profile ping and clock buffers are invalid";
        return false;
    }
    if (packageCacheDir_.empty())
    {
        if (error)
            *error = "Multiplayer profile packageCacheDir must not be empty";
        return false;
    }

    for (const auto& entry : replicationSettings_)
    {
        const ea::string name = entry.first;
        if (!IsSupportedReplicationSetting(name) || !IsSettingValueValid(name, entry.second))
        {
            if (error)
                *error = Format("Unsupported or invalid replication setting '{}'", name);
            return false;
        }
    }

    const float minDilation = replicationSettings_.find("MinTimeDilation") != replicationSettings_.end()
        ? replicationSettings_.find("MinTimeDilation")->second.GetFloat()
        : 0.7f;
    const float maxDilation = replicationSettings_.find("MaxTimeDilation") != replicationSettings_.end()
        ? replicationSettings_.find("MaxTimeDilation")->second.GetFloat()
        : 1.5f;
    if (minDilation > maxDilation)
    {
        if (error)
            *error = "Replication MinTimeDilation must not exceed MaxTimeDilation";
        return false;
    }
    return true;
}

unsigned long long MultiplayerProfileResource::ComputeDigest(ea::string* error) const
{
    if (!Validate(error))
        return 0;

    JSONFile jsonFile(context_);
    jsonFile.GetRoot() = ToJSON();
    VectorBuffer buffer;
    if (!jsonFile.Save(buffer))
    {
        if (error)
            *error = "Unable to serialize multiplayer profile for digest";
        return 0;
    }
    return HashBytes(buffer.GetData(), buffer.GetSize());
}

bool MultiplayerProfileResource::ApplyToNetwork(Network* network, ea::string* error) const
{
    if (!network)
    {
        if (error)
            *error = "Network subsystem is not available";
        return false;
    }
    if (!Validate(error))
        return false;

    network->SetUpdateFps(updateFps_);
    network->SetPingIntervalMs(pingIntervalMs_);
    network->SetMaxPingIntervalMs(maxPingIntervalMs_);
    network->SetClockBufferSize(clockBufferSize_);
    network->SetPingBufferSize(pingBufferSize_);
    network->SetPackageCacheDir(packageCacheDir_);
    return true;
}

void MultiplayerProfileResource::SerializeInBlock(Archive& archive)
{
    (void)archive;
}

bool MultiplayerProfileResource::BeginLoad(Deserializer& source)
{
    JSONFile jsonFile(context_);
    if (!jsonFile.Load(source))
        return false;
    return FromJSON(jsonFile.GetRoot());
}

bool MultiplayerProfileResource::Save(Serializer& dest) const
{
    JSONFile jsonFile(context_);
    jsonFile.GetRoot() = ToJSON();
    return jsonFile.Save(dest);
}

} // namespace Urho3D
