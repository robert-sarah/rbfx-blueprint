// Copyright (c) 2026 the rbfx-blueprint project.
//
// SPDX-License-Identifier: MIT
//

#include "PlatformExportAdapter.h"

#include <EASTL/algorithm.h>

namespace Urho3D
{

namespace
{

class LinuxExportAdapter final : public PlatformExportAdapter
{
public:
    PackagePlatform GetPlatform() const override { return PackagePlatform::Linux; }
    const char* GetName() const override { return "Linux"; }
    const PlatformExportCapabilities& GetCapabilities() const override
    {
        static const PlatformExportCapabilities capabilities = []
        {
            PlatformExportCapabilities result;
            result.supportsThreads = true;
            result.supportsGpu = true;
            result.supportsNetworking = true;
            result.supportsAot = true;
            result.supportsDynamicCode = true;
            result.architectures.push_back("x64");
            result.architectures.push_back("arm64");
            result.textureCompressions.push_back(PackageTextureCompression::None);
            result.textureCompressions.push_back(PackageTextureCompression::BC1);
            result.textureCompressions.push_back(PackageTextureCompression::BC3);
            result.textureCompressions.push_back(PackageTextureCompression::BC5);
            result.textureCompressions.push_back(PackageTextureCompression::BC7);
            return result;
        }();
        return capabilities;
    }
};

class WindowsExportAdapter final : public PlatformExportAdapter
{
public:
    PackagePlatform GetPlatform() const override { return PackagePlatform::Windows; }
    const char* GetName() const override { return "Windows"; }
    const PlatformExportCapabilities& GetCapabilities() const override
    {
        static const PlatformExportCapabilities capabilities = []
        {
            PlatformExportCapabilities result;
            result.supportsThreads = true;
            result.supportsGpu = true;
            result.supportsNetworking = true;
            result.supportsAot = true;
            result.supportsDynamicCode = true;
            result.architectures.push_back("x64");
            result.architectures.push_back("arm64");
            result.textureCompressions.push_back(PackageTextureCompression::None);
            result.textureCompressions.push_back(PackageTextureCompression::BC1);
            result.textureCompressions.push_back(PackageTextureCompression::BC3);
            result.textureCompressions.push_back(PackageTextureCompression::BC5);
            result.textureCompressions.push_back(PackageTextureCompression::BC7);
            return result;
        }();
        return capabilities;
    }
};

class MacOSExportAdapter final : public PlatformExportAdapter
{
public:
    PackagePlatform GetPlatform() const override { return PackagePlatform::macOS; }
    const char* GetName() const override { return "macOS"; }
    const PlatformExportCapabilities& GetCapabilities() const override
    {
        static const PlatformExportCapabilities capabilities = []
        {
            PlatformExportCapabilities result;
            result.supportsThreads = true;
            result.supportsGpu = true;
            result.supportsNetworking = true;
            result.supportsAot = true;
            result.supportsDynamicCode = true;
            result.architectures.push_back("x64");
            result.architectures.push_back("arm64");
            result.architectures.push_back("universal");
            result.textureCompressions.push_back(PackageTextureCompression::None);
            result.textureCompressions.push_back(PackageTextureCompression::BC7);
            result.textureCompressions.push_back(PackageTextureCompression::ASTC);
            return result;
        }();
        return capabilities;
    }
};

class AndroidExportAdapter final : public PlatformExportAdapter
{
public:
    PackagePlatform GetPlatform() const override { return PackagePlatform::Android; }
    const char* GetName() const override { return "Android"; }
    const PlatformExportCapabilities& GetCapabilities() const override
    {
        static const PlatformExportCapabilities capabilities = []
        {
            PlatformExportCapabilities result;
            result.supportsThreads = true;
            result.supportsGpu = true;
            result.supportsNetworking = true;
            result.supportsAot = true;
            result.supportsDynamicCode = false;
            result.architectures.push_back("arm64-v8a");
            result.architectures.push_back("armeabi-v7a");
            result.architectures.push_back("x86_64");
            result.textureCompressions.push_back(PackageTextureCompression::None);
            result.textureCompressions.push_back(PackageTextureCompression::ASTC);
            result.textureCompressions.push_back(PackageTextureCompression::ETC2);
            return result;
        }();
        return capabilities;
    }
};

class IOSExportAdapter final : public PlatformExportAdapter
{
public:
    PackagePlatform GetPlatform() const override { return PackagePlatform::iOS; }
    const char* GetName() const override { return "iOS"; }
    const PlatformExportCapabilities& GetCapabilities() const override
    {
        static const PlatformExportCapabilities capabilities = []
        {
            PlatformExportCapabilities result;
            result.supportsThreads = true;
            result.supportsGpu = true;
            result.supportsNetworking = true;
            result.supportsAot = true;
            result.supportsDynamicCode = false;
            result.architectures.push_back("arm64");
            result.textureCompressions.push_back(PackageTextureCompression::None);
            result.textureCompressions.push_back(PackageTextureCompression::ASTC);
            result.textureCompressions.push_back(PackageTextureCompression::ETC2);
            return result;
        }();
        return capabilities;
    }
};

class WebAssemblyExportAdapter final : public PlatformExportAdapter
{
public:
    PackagePlatform GetPlatform() const override { return PackagePlatform::WebAssembly; }
    const char* GetName() const override { return "WebAssembly"; }
    const PlatformExportCapabilities& GetCapabilities() const override
    {
        static const PlatformExportCapabilities capabilities = []
        {
            PlatformExportCapabilities result;
            result.supportsThreads = false;
            result.supportsGpu = true;
            result.supportsNetworking = true;
            result.supportsAot = true;
            result.supportsDynamicCode = false;
            result.architectures.push_back("wasm32");
            result.textureCompressions.push_back(PackageTextureCompression::None);
            result.textureCompressions.push_back(PackageTextureCompression::BC7);
            result.textureCompressions.push_back(PackageTextureCompression::ASTC);
            return result;
        }();
        return capabilities;
    }
};

const PlatformExportAdapter* GetAdapters(PackagePlatform platform)
{
    static const LinuxExportAdapter linuxAdapter;
    static const WindowsExportAdapter windowsAdapter;
    static const MacOSExportAdapter macosAdapter;
    static const AndroidExportAdapter androidAdapter;
    static const IOSExportAdapter iosAdapter;
    static const WebAssemblyExportAdapter webAssemblyAdapter;

    switch (platform)
    {
    case PackagePlatform::Linux: return &linuxAdapter;
    case PackagePlatform::Windows: return &windowsAdapter;
    case PackagePlatform::macOS: return &macosAdapter;
    case PackagePlatform::Android: return &androidAdapter;
    case PackagePlatform::iOS: return &iosAdapter;
    case PackagePlatform::WebAssembly: return &webAssemblyAdapter;
    default: return nullptr;
    }
}

} // namespace

bool PlatformExportAdapter::Validate(const PackageBuildProfile& profile, ea::string* error) const
{
    if (profile.platform != GetPlatform())
    {
        if (error)
            *error = "Package profile platform does not match the selected export adapter.";
        return false;
    }
    const auto& architectures = GetCapabilities().architectures;
    if (ea::find(architectures.begin(), architectures.end(), profile.architecture) == architectures.end())
    {
        if (error)
            *error = Format("Architecture '{}' is not supported by the {} export adapter.", profile.architecture, GetName());
        return false;
    }
    const auto& textureCompressions = GetCapabilities().textureCompressions;
    if (ea::find(textureCompressions.begin(), textureCompressions.end(), profile.textureCompression) == textureCompressions.end())
    {
        if (error)
            *error = Format("Texture compression '{}' is not supported by the {} export adapter.",
                PackageBuilder::ToString(profile.textureCompression), GetName());
        return false;
    }
    if (profile.outputPath.empty())
    {
        if (error)
            *error = "Package profile outputPath must not be empty for export.";
        return false;
    }
    return true;
}

const PlatformExportAdapter* PlatformExportAdapter::Find(PackagePlatform platform)
{
    return GetAdapters(platform);
}

StringVariantMap PlatformExportAdapter::Describe(PackagePlatform platform, bool* supported)
{
    StringVariantMap result;
    const PlatformExportAdapter* adapter = Find(platform);
    if (supported)
        *supported = adapter != nullptr;
    if (!adapter)
    {
        result["platform"] = PackageBuilder::ToString(platform);
        result["supported"] = false;
        return result;
    }

    const PlatformExportCapabilities& capabilities = adapter->GetCapabilities();
    ea::string architectures;
    for (unsigned i = 0; i < capabilities.architectures.size(); ++i)
    {
        if (i)
            architectures += ",";
        architectures += capabilities.architectures[i];
    }
    ea::string textureCompressions;
    for (unsigned i = 0; i < capabilities.textureCompressions.size(); ++i)
    {
        if (i)
            textureCompressions += ",";
        textureCompressions += PackageBuilder::ToString(capabilities.textureCompressions[i]);
    }
    result["platform"] = ea::string(adapter->GetName());
    result["supported"] = true;
    result["supportsThreads"] = capabilities.supportsThreads;
    result["supportsGpu"] = capabilities.supportsGpu;
    result["supportsNetworking"] = capabilities.supportsNetworking;
    result["supportsAot"] = capabilities.supportsAot;
    result["supportsDynamicCode"] = capabilities.supportsDynamicCode;
    result["architectures"] = architectures;
    result["textureCompressions"] = textureCompressions;
    return result;
}

} // namespace Urho3D
