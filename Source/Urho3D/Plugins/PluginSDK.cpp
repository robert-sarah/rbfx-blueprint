// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#include "PluginSDK.h"

#include <Urho3D/Core/StringUtils.h>

#include <algorithm>
#include <cstdlib>

namespace Urho3D
{

namespace
{

bool ParsePart(const ea::string& text, unsigned& value)
{
    if (text.empty())
        return false;
    char* end = nullptr;
    const unsigned long parsed = std::strtoul(text.c_str(), &end, 10);
    if (!end || *end != '\0')
        return false;
    value = static_cast<unsigned>(parsed);
    return true;
}

bool Contains(const ea::vector<ea::string>& values, const ea::string& value)
{
    return std::find(values.begin(), values.end(), value) != values.end();
}

} // namespace

bool PluginSdkVersion::Parse(const ea::string& text, PluginSdkVersion& result)
{
    const size_t first = text.find('.');
    const size_t second = first == ea::string::npos ? ea::string::npos : text.find('.', first + 1);
    if (first == ea::string::npos || second == ea::string::npos || text.find('.', second + 1) != ea::string::npos)
        return false;

    PluginSdkVersion parsed;
    if (!ParsePart(text.substr(0, first), parsed.major)
        || !ParsePart(text.substr(first + 1, second - first - 1), parsed.minor)
        || !ParsePart(text.substr(second + 1), parsed.patch))
        return false;
    result = parsed;
    return true;
}

ea::string PluginSdkVersion::ToString() const
{
    return Format("{}.{}.{}", major, minor, patch);
}

bool PluginSdkVersion::IsCompatibleWith(const PluginSdkVersion& required) const
{
    return major == required.major && (minor > required.minor || (minor == required.minor && patch >= required.patch));
}

void PluginSdk::SetError(ea::string* error, const ea::string& message) const
{
    lastError_ = message;
    if (error)
        *error = message;
}

const PluginSdkModule* PluginSdk::Find(const ea::string& name) const
{
    for (const PluginSdkModule& module : modules_)
    {
        if (module.descriptor.name == name)
            return &module;
    }
    return nullptr;
}

bool PluginSdk::ValidateModule(const PluginSdkModule& module, const PluginSdkVersion& engineVersion,
    unsigned sdkAbi, ea::string* error) const
{
    if (module.descriptor.name.empty())
    {
        SetError(error, "Plugin SDK module name must not be empty.");
        return false;
    }
    PluginSdkVersion moduleVersion;
    if (!PluginSdkVersion::Parse(module.descriptor.version, moduleVersion))
    {
        SetError(error, Format("Plugin SDK module '{}' has an invalid semantic version.", module.descriptor.name));
        return false;
    }
    if (module.sdkAbi != sdkAbi)
    {
        SetError(error, Format("Plugin SDK module '{}' requires ABI {}, but ABI {} is active.",
            module.descriptor.name, module.sdkAbi, sdkAbi));
        return false;
    }
    if (!engineVersion.IsCompatibleWith(module.minimumEngineVersion))
    {
        SetError(error, Format("Plugin SDK module '{}' requires engine {}, but engine {} is active.",
            module.descriptor.name, module.minimumEngineVersion.ToString(), engineVersion.ToString()));
        return false;
    }
    for (const ea::string& dependency : module.descriptor.dependencies)
    {
        if (!Find(dependency))
        {
            SetError(error, Format("Plugin SDK module '{}' depends on missing module '{}'.", module.descriptor.name, dependency));
            return false;
        }
    }
    return true;
}

bool PluginSdk::RegisterModule(const PluginSdkModule& module, ea::string* error)
{
    if (Find(module.descriptor.name))
    {
        SetError(error, Format("Plugin SDK module '{}' is already registered.", module.descriptor.name));
        return false;
    }
    if (module.descriptor.name.empty() || module.descriptor.version.empty())
    {
        SetError(error, "Plugin SDK module must provide a name and semantic version.");
        return false;
    }
    PluginSdkVersion ignored;
    if (!PluginSdkVersion::Parse(module.descriptor.version, ignored))
    {
        SetError(error, Format("Plugin SDK module '{}' has an invalid semantic version.", module.descriptor.name));
        return false;
    }
    modules_.push_back(module);
    std::sort(modules_.begin(), modules_.end(), [](const PluginSdkModule& lhs, const PluginSdkModule& rhs)
    {
        return lhs.descriptor.name < rhs.descriptor.name;
    });
    return true;
}

bool PluginSdk::UnregisterModule(const ea::string& name, ea::string* error)
{
    if (Contains(activeModules_, name))
    {
        SetError(error, Format("Plugin SDK module '{}' must be deactivated before removal.", name));
        return false;
    }
    auto it = std::find_if(modules_.begin(), modules_.end(), [&name](const PluginSdkModule& module)
    {
        return module.descriptor.name == name;
    });
    if (it == modules_.end())
    {
        SetError(error, Format("Plugin SDK module '{}' is not registered.", name));
        return false;
    }
    for (const PluginSdkModule& module : modules_)
    {
        if (Contains(module.descriptor.dependencies, name))
        {
            SetError(error, Format("Plugin SDK module '{}' is still required by '{}'.", name, module.descriptor.name));
            return false;
        }
    }
    modules_.erase(it);
    return true;
}

bool PluginSdk::Validate(const PluginSdkVersion& engineVersion, unsigned sdkAbi, ea::string* error) const
{
    for (const PluginSdkModule& module : modules_)
    {
        if (!ValidateModule(module, engineVersion, sdkAbi, error))
            return false;
    }

    ea::unordered_map<ea::string, unsigned char> marks;
    ea::vector<ea::string> order;
    auto visit = [&](auto&& self, const ea::string& name) -> bool
    {
        unsigned char& mark = marks[name];
        if (mark == 1)
        {
            SetError(error, Format("Plugin SDK dependency cycle includes '{}'.", name));
            return false;
        }
        if (mark == 2)
            return true;
        const PluginSdkModule* module = Find(name);
        if (!module)
        {
            SetError(error, Format("Plugin SDK dependency '{}' is not registered.", name));
            return false;
        }
        mark = 1;
        ea::vector<ea::string> dependencies = module->descriptor.dependencies;
        std::sort(dependencies.begin(), dependencies.end());
        for (const ea::string& dependency : dependencies)
        {
            if (!self(self, dependency))
                return false;
        }
        mark = 2;
        order.push_back(name);
        return true;
    };

    ea::vector<ea::string> names;
    for (const PluginSdkModule& module : modules_)
        names.push_back(module.descriptor.name);
    std::sort(names.begin(), names.end());
    for (const ea::string& name : names)
    {
        if (!visit(visit, name))
            return false;
    }
    return true;
}

bool PluginSdk::ActivateAll(const PluginSdkVersion& engineVersion, unsigned sdkAbi, ea::string* error)
{
    DeactivateAll();
    if (!Validate(engineVersion, sdkAbi, error))
        return false;

    ea::unordered_map<ea::string, unsigned char> marks;
    ea::vector<const PluginSdkModule*> order;
    auto visit = [&](auto&& self, const ea::string& name) -> bool
    {
        unsigned char& mark = marks[name];
        if (mark == 1)
            return false;
        if (mark == 2)
            return true;
        const PluginSdkModule* module = Find(name);
        if (!module)
            return false;
        mark = 1;
        ea::vector<ea::string> dependencies = module->descriptor.dependencies;
        std::sort(dependencies.begin(), dependencies.end());
        for (const ea::string& dependency : dependencies)
        {
            if (!self(self, dependency))
                return false;
        }
        mark = 2;
        order.push_back(module);
        return true;
    };

    ea::vector<ea::string> names;
    for (const PluginSdkModule& module : modules_)
    {
        if (module.descriptor.enabled)
            names.push_back(module.descriptor.name);
    }
    std::sort(names.begin(), names.end());
    for (const ea::string& name : names)
    {
        if (!visit(visit, name))
        {
            SetError(error, "Plugin SDK activation order could not be resolved.");
            DeactivateAll();
            return false;
        }
    }

    for (const PluginSdkModule* module : order)
    {
        const PluginSdkRegistration& registration = module->registration;
        const ea::function<bool(Context*, ea::string*)> callbacks[] =
            {registration.registerReflection, registration.registerBlueprint, registration.registerRbScript, registration.registerEditor};
        for (const auto& callback : callbacks)
        {
            if (callback && !callback(context_, error))
            {
                DeactivateAll();
                return false;
            }
        }
        activeModules_.push_back(module->descriptor.name);
    }
    return true;
}

void PluginSdk::DeactivateAll()
{
    for (auto it = activeModules_.rbegin(); it != activeModules_.rend(); ++it)
    {
        if (const PluginSdkModule* module = Find(*it))
        {
            if (module->registration.unregister)
                module->registration.unregister(context_);
        }
    }
    activeModules_.clear();
}

} // namespace Urho3D
