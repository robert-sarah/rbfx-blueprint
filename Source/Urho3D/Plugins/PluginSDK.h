// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <Urho3D/Urho3D.h>
#include <Urho3D/WorldFabric/PluginRegistry.h>

#include <EASTL/functional.h>
#include <EASTL/vector.h>

namespace Urho3D
{

struct URHO3D_API PluginSdkVersion
{
    unsigned major{};
    unsigned minor{};
    unsigned patch{};

    static bool Parse(const ea::string& text, PluginSdkVersion& result);
    ea::string ToString() const;
    bool IsCompatibleWith(const PluginSdkVersion& required) const;
};

/// Public extension points exposed to a native or tooling plugin.
struct URHO3D_API PluginSdkRegistration
{
    ea::function<bool(Context*, ea::string*)> registerReflection;
    ea::function<bool(Context*, ea::string*)> registerBlueprint;
    ea::function<bool(Context*, ea::string*)> registerRbScript;
    ea::function<bool(Context*, ea::string*)> registerEditor;
    ea::function<void(Context*)> unregister;
};

/// ABI-stable metadata and callbacks for one public rbfx-blueprint extension.
struct URHO3D_API PluginSdkModule
{
    PluginDescriptor descriptor;
    unsigned sdkAbi{1};
    PluginSdkVersion minimumEngineVersion;
    PluginSdkRegistration registration;
};

/// Validates, orders and activates public SDK modules without owning dynamic libraries.
/// Dynamic loading remains the responsibility of PluginManager/ModulePlugin; this class
/// provides the stable contract shared by native modules, tools and editor integrations.
class URHO3D_API PluginSdk
{
public:
    explicit PluginSdk(Context* context = nullptr) : context_(context) {}

    void SetContext(Context* context) { context_ = context; }
    Context* GetContext() const { return context_; }

    bool RegisterModule(const PluginSdkModule& module, ea::string* error = nullptr);
    bool UnregisterModule(const ea::string& name, ea::string* error = nullptr);
    bool Validate(const PluginSdkVersion& engineVersion, unsigned sdkAbi, ea::string* error = nullptr) const;
    bool ActivateAll(const PluginSdkVersion& engineVersion, unsigned sdkAbi, ea::string* error = nullptr);
    void DeactivateAll();

    const ea::vector<PluginSdkModule>& GetModules() const { return modules_; }
    const ea::vector<ea::string>& GetActiveModules() const { return activeModules_; }
    const ea::string& GetLastError() const { return lastError_; }

private:
    void SetError(ea::string* error, const ea::string& message) const;
    const PluginSdkModule* Find(const ea::string& name) const;
    bool ValidateModule(const PluginSdkModule& module, const PluginSdkVersion& engineVersion,
        unsigned sdkAbi, ea::string* error) const;

    Context* context_{};
    ea::vector<PluginSdkModule> modules_;
    ea::vector<ea::string> activeModules_;
    mutable ea::string lastError_;
};

} // namespace Urho3D
