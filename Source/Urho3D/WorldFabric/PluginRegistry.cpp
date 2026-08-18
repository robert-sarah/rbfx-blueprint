#include "../Precompiled.h"

#include "PluginRegistry.h"

#include "../IO/Deserializer.h"
#include "../IO/Serializer.h"
#include "../Resource/JSONFile.h"

#include <EASTL/algorithm.h>

#include <algorithm>
#include <cctype>

namespace Urho3D
{

namespace
{

bool IsIdentifierCharacter(char character)
{
    const unsigned char value = static_cast<unsigned char>(character);
    return std::isalnum(value) || character == '.' || character == '-' || character == '_';
}

bool IsSemVerIdentifier(const ea::string& identifier, bool numeric)
{
    if (identifier.empty())
        return false;

    for (const char character : identifier)
    {
        const unsigned char value = static_cast<unsigned char>(character);
        if (!std::isalnum(value) && character != '-')
            return false;
    }

    if (numeric && identifier.size() > 1 && identifier[0] == '0')
        return false;
    return true;
}

bool IsSemanticVersion(const ea::string& version)
{
    const size_t plus = version.find('+');
    const size_t dash = version.find('-');
    const size_t coreEnd = dash != ea::string::npos ? dash : (plus != ea::string::npos ? plus : version.size());
    if (coreEnd == 0)
        return false;

    const ea::string core = version.substr(0, coreEnd);
    ea::vector<ea::string> coreParts = core.split('.');
    if (coreParts.size() != 3)
        return false;
    for (const ea::string& part : coreParts)
    {
        if (!IsSemVerIdentifier(part, true))
            return false;
        for (const char character : part)
        {
            if (!std::isdigit(static_cast<unsigned char>(character)))
                return false;
        }
    }

    if (dash != ea::string::npos)
    {
        const size_t prereleaseEnd = plus != ea::string::npos ? plus : version.size();
        const ea::string prerelease = version.substr(dash + 1, prereleaseEnd - dash - 1);
        const ea::vector<ea::string> identifiers = prerelease.split('.');
        if (identifiers.empty())
            return false;
        for (const ea::string& identifier : identifiers)
        {
            const bool numeric = !identifier.empty()
                && std::all_of(identifier.begin(), identifier.end(), [](char character)
                    { return std::isdigit(static_cast<unsigned char>(character)) != 0; });
            if (!IsSemVerIdentifier(identifier, numeric))
                return false;
        }
    }

    if (plus != ea::string::npos)
    {
        const ea::string build = version.substr(plus + 1);
        const ea::vector<ea::string> identifiers = build.split('.');
        if (identifiers.empty())
            return false;
        for (const ea::string& identifier : identifiers)
        {
            if (!IsSemVerIdentifier(identifier, false))
                return false;
        }
    }

    return true;
}

bool HasDuplicate(const ea::vector<ea::string>& values, ea::string* duplicate)
{
    for (unsigned i = 0; i < values.size(); ++i)
    {
        for (unsigned j = i + 1; j < values.size(); ++j)
        {
            if (values[i] == values[j])
            {
                if (duplicate)
                    *duplicate = values[i];
                return true;
            }
        }
    }
    return false;
}

} // namespace

PluginRegistry::PluginRegistry(Context* context)
    : Resource(context)
{
}

bool PluginRegistry::CheckExtension(const ea::string& fileName)
{
    return fileName.ends_with(".pluginregistry", false) || fileName.ends_with(".plugins", false)
        || fileName.ends_with(".pluginmanifest", false);
}

bool PluginRegistry::AddPlugin(const PluginDescriptor& descriptor, ea::string* error)
{
    if (!ValidateDescriptor(descriptor, error))
        return false;
    if (FindPlugin(descriptor.name))
    {
        if (error)
            *error = Format("Plugin '{}' is already registered", descriptor.name);
        return false;
    }
    plugins_.push_back(descriptor);
    return true;
}

bool PluginRegistry::RemovePlugin(const ea::string& name, ea::string* error)
{
    for (auto i = plugins_.begin(); i != plugins_.end(); ++i)
    {
        if (i->name == name)
        {
            plugins_.erase(i);
            return true;
        }
    }

    if (error)
        *error = Format("Plugin '{}' is not registered", name);
    return false;
}

PluginDescriptor* PluginRegistry::FindPlugin(const ea::string& name)
{
    for (PluginDescriptor& descriptor : plugins_)
    {
        if (descriptor.name == name)
            return &descriptor;
    }
    return nullptr;
}

const PluginDescriptor* PluginRegistry::FindPlugin(const ea::string& name) const
{
    for (const PluginDescriptor& descriptor : plugins_)
    {
        if (descriptor.name == name)
            return &descriptor;
    }
    return nullptr;
}

bool PluginRegistry::ValidateDescriptor(const PluginDescriptor& descriptor, ea::string* error) const
{
    if (descriptor.name.empty())
    {
        if (error)
            *error = "Plugin name must not be empty";
        return false;
    }
    for (const char character : descriptor.name)
    {
        if (!IsIdentifierCharacter(character))
        {
            if (error)
                *error = Format("Plugin name '{}' contains an invalid character", descriptor.name);
            return false;
        }
    }
    if (!IsSemanticVersion(descriptor.version))
    {
        if (error)
            *error = Format("Plugin '{}' has an invalid semantic version '{}'", descriptor.name, descriptor.version);
        return false;
    }
    if (!descriptor.entryPoint.empty())
    {
        for (const char character : descriptor.entryPoint)
        {
            if (!IsIdentifierCharacter(character))
            {
                if (error)
                    *error = Format("Plugin '{}' has an invalid entry point", descriptor.name);
                return false;
            }
        }
    }
    if (descriptor.binary.find('\n') != ea::string::npos || descriptor.binary.find('\r') != ea::string::npos)
    {
        if (error)
            *error = Format("Plugin '{}' binary path contains a line break", descriptor.name);
        return false;
    }

    ea::string duplicate;
    if (HasDuplicate(descriptor.capabilities, &duplicate))
    {
        if (error)
            *error = Format("Plugin '{}' declares capability '{}' more than once", descriptor.name, duplicate);
        return false;
    }
    for (const ea::string& capability : descriptor.capabilities)
    {
        if (capability.empty())
        {
            if (error)
                *error = Format("Plugin '{}' contains an empty capability", descriptor.name);
            return false;
        }
    }
    if (HasDuplicate(descriptor.dependencies, &duplicate))
    {
        if (error)
            *error = Format("Plugin '{}' depends on '{}' more than once", descriptor.name, duplicate);
        return false;
    }
    for (const ea::string& dependency : descriptor.dependencies)
    {
        if (dependency.empty() || dependency == descriptor.name)
        {
            if (error)
                *error = Format("Plugin '{}' contains an invalid dependency", descriptor.name);
            return false;
        }
    }
    return true;
}

bool PluginRegistry::Validate(ea::string* error) const
{
    for (unsigned i = 0; i < plugins_.size(); ++i)
    {
        if (!ValidateDescriptor(plugins_[i], error))
            return false;
        for (unsigned j = i + 1; j < plugins_.size(); ++j)
        {
            if (plugins_[i].name == plugins_[j].name)
            {
                if (error)
                    *error = Format("Plugin name '{}' must be unique", plugins_[i].name);
                return false;
            }
        }
    }

    for (const PluginDescriptor& descriptor : plugins_)
    {
        for (const ea::string& dependency : descriptor.dependencies)
        {
            const PluginDescriptor* dependencyDescriptor = FindPlugin(dependency);
            if (!dependencyDescriptor)
            {
                if (error)
                    *error = Format("Plugin '{}' depends on missing plugin '{}'", descriptor.name, dependency);
                return false;
            }
            if (descriptor.enabled && !dependencyDescriptor->enabled)
            {
                if (error)
                    *error = Format("Enabled plugin '{}' depends on disabled plugin '{}'", descriptor.name, dependency);
                return false;
            }
        }
    }

    ea::string orderError;
    GetLoadOrder(&orderError);
    if (!orderError.empty())
    {
        if (error)
            *error = orderError;
        return false;
    }
    return true;
}

bool PluginRegistry::VisitForLoadOrder(unsigned index, ea::vector<unsigned char>& marks,
    ea::vector<ea::string>& order, ea::string* error) const
{
    if (marks[index] == 2 || !plugins_[index].enabled)
        return true;
    if (marks[index] == 1)
    {
        if (error)
            *error = Format("Plugin dependency cycle detected at '{}'", plugins_[index].name);
        return false;
    }

    marks[index] = 1;
    ea::vector<unsigned> dependencies;
    for (const ea::string& dependency : plugins_[index].dependencies)
    {
        for (unsigned dependencyIndex = 0; dependencyIndex < plugins_.size(); ++dependencyIndex)
        {
            if (plugins_[dependencyIndex].name == dependency && plugins_[dependencyIndex].enabled)
            {
                dependencies.push_back(dependencyIndex);
                break;
            }
        }
    }
    std::sort(dependencies.begin(), dependencies.end(), [this](unsigned left, unsigned right)
        { return plugins_[left].name < plugins_[right].name; });
    for (const unsigned dependencyIndex : dependencies)
    {
        if (!VisitForLoadOrder(dependencyIndex, marks, order, error))
            return false;
    }

    marks[index] = 2;
    order.push_back(plugins_[index].name);
    return true;
}

ea::vector<ea::string> PluginRegistry::GetLoadOrder(ea::string* error) const
{
    ea::vector<ea::string> order;
    if (plugins_.empty())
        return order;

    ea::vector<unsigned> indices;
    indices.reserve(plugins_.size());
    for (unsigned i = 0; i < plugins_.size(); ++i)
        indices.push_back(i);
    std::sort(indices.begin(), indices.end(), [this](unsigned left, unsigned right)
        { return plugins_[left].name < plugins_[right].name; });

    ea::vector<unsigned char> marks(plugins_.size(), 0);
    for (const unsigned index : indices)
    {
        if (!plugins_[index].enabled)
            continue;
        if (!VisitForLoadOrder(index, marks, order, error))
            return {};
    }
    return order;
}

JSONValue PluginRegistry::ToJSON() const
{
    JSONValue root(JSON_OBJECT);
    root.Set("version", 1u);

    ea::vector<unsigned> indices;
    indices.reserve(plugins_.size());
    for (unsigned i = 0; i < plugins_.size(); ++i)
        indices.push_back(i);
    std::sort(indices.begin(), indices.end(), [this](unsigned left, unsigned right)
        { return plugins_[left].name < plugins_[right].name; });

    JSONValue plugins(JSON_ARRAY);
    for (const unsigned index : indices)
    {
        const PluginDescriptor& descriptor = plugins_[index];
        JSONValue plugin(JSON_OBJECT);
        plugin.Set("name", descriptor.name);
        plugin.Set("version", descriptor.version);
        plugin.Set("entryPoint", descriptor.entryPoint);
        plugin.Set("binary", descriptor.binary);
        plugin.Set("static", descriptor.staticPlugin);
        plugin.Set("enabled", descriptor.enabled);

        ea::vector<ea::string> capabilities = descriptor.capabilities;
        std::sort(capabilities.begin(), capabilities.end());
        JSONValue capabilityArray(JSON_ARRAY);
        for (const ea::string& capability : capabilities)
            capabilityArray.Push(capability);
        plugin.Set("capabilities", ea::move(capabilityArray));

        ea::vector<ea::string> dependencies = descriptor.dependencies;
        std::sort(dependencies.begin(), dependencies.end());
        JSONValue dependencyArray(JSON_ARRAY);
        for (const ea::string& dependency : dependencies)
            dependencyArray.Push(dependency);
        plugin.Set("dependencies", ea::move(dependencyArray));

        JSONValue metadata(JSON_OBJECT);
        metadata.SetStringVariantMap(descriptor.metadata, context_);
        plugin.Set("metadata", ea::move(metadata));
        plugins.Push(ea::move(plugin));
    }
    root.Set("plugins", ea::move(plugins));
    return root;
}

bool PluginRegistry::FromJSON(const JSONValue& root, ea::string* error)
{
    if (!root.IsObject() || !root.Get("version").IsNumber() || root.Get("version").GetUInt() != 1u
        || !root.Get("plugins").IsArray())
    {
        if (error)
            *error = "Plugin registry requires version 1 and a plugins array";
        return false;
    }

    ea::vector<PluginDescriptor> candidate;
    for (const JSONValue& pluginJson : root.Get("plugins").GetArray())
    {
        if (!pluginJson.IsObject() || !pluginJson.Get("name").IsString() || !pluginJson.Get("version").IsString()
            || !pluginJson.Get("entryPoint").IsString() || !pluginJson.Get("binary").IsString()
            || !pluginJson.Get("static").IsBool() || !pluginJson.Get("enabled").IsBool()
            || !pluginJson.Get("capabilities").IsArray() || !pluginJson.Get("dependencies").IsArray()
            || !pluginJson.Get("metadata").IsObject())
        {
            if (error)
                *error = "Plugin registry contains an invalid descriptor";
            return false;
        }

        PluginDescriptor descriptor;
        descriptor.name = pluginJson.Get("name").GetString();
        descriptor.version = pluginJson.Get("version").GetString();
        descriptor.entryPoint = pluginJson.Get("entryPoint").GetString();
        descriptor.binary = pluginJson.Get("binary").GetString();
        descriptor.staticPlugin = pluginJson.Get("static").GetBool();
        descriptor.enabled = pluginJson.Get("enabled").GetBool();
        descriptor.metadata = pluginJson.Get("metadata").GetStringVariantMap();

        for (const JSONValue& capability : pluginJson.Get("capabilities").GetArray())
        {
            if (!capability.IsString() || capability.GetString().empty())
            {
                if (error)
                    *error = Format("Plugin '{}' contains an invalid capability", descriptor.name);
                return false;
            }
            descriptor.capabilities.push_back(capability.GetString());
        }
        for (const JSONValue& dependency : pluginJson.Get("dependencies").GetArray())
        {
            if (!dependency.IsString() || dependency.GetString().empty())
            {
                if (error)
                    *error = Format("Plugin '{}' contains an invalid dependency", descriptor.name);
                return false;
            }
            descriptor.dependencies.push_back(dependency.GetString());
        }
        if (!ValidateDescriptor(descriptor, error))
            return false;
        candidate.push_back(ea::move(descriptor));
    }

    PluginRegistry candidateRegistry(context_);
    candidateRegistry.plugins_ = ea::move(candidate);
    if (!candidateRegistry.Validate(error))
        return false;

    plugins_ = ea::move(candidateRegistry.plugins_);
    return true;
}

unsigned long long PluginRegistry::ComputeDigest(ea::string* error) const
{
    if (!Validate(error))
        return 0;

    JSONFile jsonFile(context_);
    jsonFile.GetRoot() = ToJSON();
    const ea::string serialized = jsonFile.ToString("");

    unsigned long long digest = 1469598103934665603ull;
    for (const unsigned char character : serialized)
    {
        digest ^= character;
        digest *= 1099511628211ull;
    }
    return digest;
}

void PluginRegistry::SerializeInBlock(Archive& archive)
{
    (void)archive;
}

bool PluginRegistry::BeginLoad(Deserializer& source)
{
    JSONFile jsonFile(context_);
    if (!jsonFile.Load(source))
        return false;
    return FromJSON(jsonFile.GetRoot());
}

bool PluginRegistry::Save(Serializer& dest) const
{
    JSONFile jsonFile(context_);
    jsonFile.GetRoot() = ToJSON();
    return jsonFile.Save(dest);
}

} // namespace Urho3D
