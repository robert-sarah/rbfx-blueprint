#pragma once

#include <Urho3D/Core/Variant.h>
#include <Urho3D/Resource/JSONValue.h>
#include <Urho3D/Resource/Resource.h>

namespace Urho3D
{

/// Persistent descriptor for an engine plugin and its production metadata.
struct URHO3D_API PluginDescriptor
{
    /// Stable plugin identifier. It is also the name used by PluginManager.
    ea::string name;
    /// Semantic version of the plugin descriptor.
    ea::string version;
    /// Optional registered PluginApplication name.
    ea::string entryPoint;
    /// Optional dynamic module path or package binary.
    ea::string binary;
    /// Whether the plugin is implemented as a static PluginApplication.
    bool staticPlugin{};
    /// Whether the plugin is enabled for the current registry profile.
    bool enabled{true};
    /// Declared capabilities such as Rendering, Scripting or Editor.
    ea::vector<ea::string> capabilities;
    /// Plugin identifiers that must be loaded before this plugin.
    ea::vector<ea::string> dependencies;
    /// Extensible package and tooling metadata.
    StringVariantMap metadata;
};

/// JSON-backed plugin descriptor registry used by the P3 SDK and package pipeline.
class URHO3D_API PluginRegistry : public Resource
{
    URHO3D_OBJECT(PluginRegistry, Resource);

public:
    explicit PluginRegistry(Context* context);

    /// Return whether a file name uses a supported plugin registry extension.
    static bool CheckExtension(const ea::string& fileName);

    /// Remove all descriptors.
    void Reset() { plugins_.clear(); }
    /// Return editable plugin descriptors.
    ea::vector<PluginDescriptor>& GetPlugins() { return plugins_; }
    /// Return plugin descriptors.
    const ea::vector<PluginDescriptor>& GetPlugins() const { return plugins_; }

    /// Add a descriptor if its identity does not already exist.
    bool AddPlugin(const PluginDescriptor& descriptor, ea::string* error = nullptr);
    /// Remove a descriptor by stable identifier.
    bool RemovePlugin(const ea::string& name, ea::string* error = nullptr);
    /// Find a descriptor by stable identifier.
    PluginDescriptor* FindPlugin(const ea::string& name);
    const PluginDescriptor* FindPlugin(const ea::string& name) const;

    /// Validate identities, versions, dependencies, capabilities and cycles.
    bool Validate(ea::string* error = nullptr) const;
    /// Return enabled plugins in deterministic dependency order.
    ea::vector<ea::string> GetLoadOrder(ea::string* error = nullptr) const;
    /// Compute a deterministic digest of the validated registry.
    unsigned long long ComputeDigest(ea::string* error = nullptr) const;

    /// Serialize the registry to its canonical JSON representation.
    JSONValue ToJSON() const;
    /// Load and validate a registry from JSON without partially mutating it.
    bool FromJSON(const JSONValue& root, ea::string* error = nullptr);

    void SerializeInBlock(Archive& archive) override;
    bool BeginLoad(Deserializer& source) override;
    bool Save(Serializer& dest) const override;

private:
    bool ValidateDescriptor(const PluginDescriptor& descriptor, ea::string* error) const;
    bool VisitForLoadOrder(unsigned index, ea::vector<unsigned char>& marks,
        ea::vector<ea::string>& order, ea::string* error) const;

    ea::vector<PluginDescriptor> plugins_;
};

} // namespace Urho3D
