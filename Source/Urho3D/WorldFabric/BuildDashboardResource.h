// SPDX-License-Identifier: MIT
#pragma once

#include <Urho3D/Resource/JSONValue.h>
#include <Urho3D/Resource/Resource.h>
#include <Urho3D/WorldFabric/BuildGraph.h>

namespace Urho3D
{

/// JSON-backed production configuration for the deterministic BuildGraph scheduler.
class URHO3D_API BuildDashboardResource : public Resource
{
    URHO3D_OBJECT(BuildDashboardResource, Resource);

public:
    explicit BuildDashboardResource(Context* context);

    static bool CheckExtension(const ea::string& fileName);
    static const char* GetBuildTaskKindName(BuildTaskKind kind);
    static bool ParseBuildTaskKind(const ea::string& name, BuildTaskKind& kind);

    const ea::string& GetPlatform() const { return platform_; }
    const ea::string& GetConfiguration() const { return configuration_; }
    void SetPlatform(const ea::string& platform) { platform_ = platform; }
    void SetConfiguration(const ea::string& configuration) { configuration_ = configuration; }

    const ea::vector<BuildTask>& GetTasks() const { return tasks_; }
    ea::vector<BuildTask>& GetTasks() { return tasks_; }
    void SetTasks(const ea::vector<BuildTask>& tasks) { tasks_ = tasks; }

    JSONValue ToJSON() const;
    bool FromJSON(const JSONValue& root, ea::string* error = nullptr);
    bool Validate(ea::string* error = nullptr) const;
    ea::vector<ea::string> GetBuildOrder(ea::string* error = nullptr) const;
    unsigned long long ComputeDigest(ea::string* error = nullptr) const;

    void SerializeInBlock(Archive& archive) override;
    bool BeginLoad(Deserializer& source) override;
    bool Save(Serializer& dest) const override;

private:
    BuildGraph CreateValidationGraph() const;

    ea::string platform_{"Linux"};
    ea::string configuration_{"Debug"};
    ea::vector<BuildTask> tasks_;
};

} // namespace Urho3D
