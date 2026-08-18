#pragma once

#include <Urho3D/Resource/JSONValue.h>
#include <Urho3D/Resource/Resource.h>

namespace Urho3D
{

class Network;

/// Persisted production profile for server/client startup and replication tuning.
class URHO3D_API MultiplayerProfileResource : public Resource
{
    URHO3D_OBJECT(MultiplayerProfileResource, Resource);

public:
    enum class Mode
    {
        Server,
        Client,
        ListenServer
    };

    explicit MultiplayerProfileResource(Context* context);

    static bool CheckExtension(const ea::string& fileName);
    static const char* GetModeName(Mode mode);
    static bool ParseMode(const ea::string& name, Mode& mode);

    Mode GetMode() const { return mode_; }
    void SetMode(Mode mode) { mode_ = mode; }

    const ea::string& GetAddress() const { return address_; }
    void SetAddress(const ea::string& address) { address_ = address; }

    unsigned GetPort() const { return port_; }
    void SetPort(unsigned port) { port_ = port; }

    unsigned GetMaxConnections() const { return maxConnections_; }
    void SetMaxConnections(unsigned value) { maxConnections_ = value; }

    unsigned GetUpdateFps() const { return updateFps_; }
    void SetUpdateFps(unsigned value) { updateFps_ = value; }

    unsigned GetPingIntervalMs() const { return pingIntervalMs_; }
    void SetPingIntervalMs(unsigned value) { pingIntervalMs_ = value; }

    unsigned GetMaxPingIntervalMs() const { return maxPingIntervalMs_; }
    void SetMaxPingIntervalMs(unsigned value) { maxPingIntervalMs_ = value; }

    unsigned GetClockBufferSize() const { return clockBufferSize_; }
    void SetClockBufferSize(unsigned value) { clockBufferSize_ = value; }

    unsigned GetPingBufferSize() const { return pingBufferSize_; }
    void SetPingBufferSize(unsigned value) { pingBufferSize_ = value; }

    const ea::string& GetPackageCacheDir() const { return packageCacheDir_; }
    void SetPackageCacheDir(const ea::string& value) { packageCacheDir_ = value; }

    const StringVariantMap& GetReplicationSettings() const { return replicationSettings_; }
    StringVariantMap& GetReplicationSettings() { return replicationSettings_; }
    void SetReplicationSettings(const StringVariantMap& value) { replicationSettings_ = value; }

    JSONValue ToJSON() const;
    bool FromJSON(const JSONValue& root, ea::string* error = nullptr);
    bool Validate(ea::string* error = nullptr) const;
    unsigned long long ComputeDigest(ea::string* error = nullptr) const;

    /// Apply non-destructive transport and tuning settings to a live Network subsystem.
    bool ApplyToNetwork(Network* network, ea::string* error = nullptr) const;

    void SerializeInBlock(Archive& archive) override;
    bool BeginLoad(Deserializer& source) override;
    bool Save(Serializer& dest) const override;

private:
    static bool IsSupportedReplicationSetting(const ea::string& name);
    static bool IsSettingValueValid(const ea::string& name, const Variant& value);

    Mode mode_{Mode::ListenServer};
    ea::string address_{"127.0.0.1"};
    unsigned port_{2345};
    unsigned maxConnections_{128};
    unsigned updateFps_{30};
    unsigned pingIntervalMs_{250};
    unsigned maxPingIntervalMs_{10000};
    unsigned clockBufferSize_{40};
    unsigned pingBufferSize_{10};
    ea::string packageCacheDir_{"Cache/Packages"};
    StringVariantMap replicationSettings_;
};

} // namespace Urho3D
