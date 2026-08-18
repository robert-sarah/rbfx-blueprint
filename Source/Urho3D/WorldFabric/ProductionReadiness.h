// SPDX-License-Identifier: MIT
#pragma once

#include <Urho3D/Urho3D.h>
#include <Urho3D/WorldFabric/ProductionFinalization.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace Urho3D
{

enum class ProductionDiagnosticSeverity
{
    Info,
    Warning,
    Error,
    Critical
};

struct ProductionDiagnostic
{
    std::string code;
    ProductionDiagnosticSeverity severity{ProductionDiagnosticSeverity::Info};
    std::string source;
    std::string message;
    unsigned line{};
    unsigned column{};
};

/// Deterministic, bounded diagnostics suitable for editor reports and CI artifacts.
class URHO3D_API ProductionDiagnosticLog
{
public:
    explicit ProductionDiagnosticLog(std::size_t capacity = 4096);
    bool Add(const ProductionDiagnostic& diagnostic);
    bool HasErrors() const;
    bool HasCritical() const;
    std::size_t Count(ProductionDiagnosticSeverity severity) const;
    std::string Serialize() const;
    void Clear();
    const std::vector<ProductionDiagnostic>& GetRecords() const { return records_; }

private:
    static std::string RedactSensitive(const std::string& text);
    std::size_t capacity_{};
    std::vector<ProductionDiagnostic> records_;
};

struct SanitizerEvidence
{
    bool address{};
    bool undefinedBehavior{};
    bool thread{};
    bool leak{};
    bool staticAnalysis{};
    bool fuzzing{};
    std::string toolchain;
    std::string diagnostic;
};

class URHO3D_API ProductionSanitizerGate
{
public:
    bool Validate(const SanitizerEvidence& evidence, std::string* error = nullptr) const;
};

struct SoakEvidence
{
    std::uint64_t frames{};
    double durationSeconds{};
    double maxFrameMilliseconds{};
    std::uint64_t maxMemoryMegabytes{};
    std::uint64_t crashCount{};
    std::uint64_t dataRaceCount{};
    std::uint64_t invalidResourceCount{};
};

struct SoakBudget
{
    std::uint64_t minimumFrames{};
    double minimumDurationSeconds{};
    double maximumFrameMilliseconds{};
    std::uint64_t maximumMemoryMegabytes{};
};

class URHO3D_API ProductionSoakGate
{
public:
    bool Validate(const SoakBudget& budget, const SoakEvidence& evidence, std::string* error = nullptr) const;
};

struct NativePlatformEvidence
{
    ProductionPlatform platform{ProductionPlatform::Linux};
    std::string architecture;
    std::string compiler;
    std::string graphicsBackend;
    std::string artifactDigest;
    bool configured{};
    bool compiled{};
    bool testsExecuted{};
    bool graphicalSmoke{};
    bool installTested{};
    bool packageVerified{};
};

class URHO3D_API ProductionNativeReleaseGate
{
public:
    bool Add(const NativePlatformEvidence& evidence);
    const NativePlatformEvidence* Find(ProductionPlatform platform) const;
    bool ValidateDesktopMatrix(std::string* error = nullptr) const;
    void Clear();
    const std::vector<NativePlatformEvidence>& GetEntries() const { return entries_; }

private:
    std::vector<NativePlatformEvidence> entries_;
};

struct ReproducibleBuildSpec
{
    std::string sourceCommit;
    std::string compiler;
    std::string compilerVersion;
    std::string cmakeVersion;
    std::string ninjaVersion;
    std::string toolchain;
    std::string buildType;
    std::string dependencyDigest;
    std::string sourceDigest;
    std::string artifactDigest;
};

class URHO3D_API ReproducibleBuildVerifier
{
public:
    bool Validate(const ReproducibleBuildSpec& spec, std::string* error = nullptr) const;
    bool Equivalent(const ReproducibleBuildSpec& left, const ReproducibleBuildSpec& right) const;
    std::uint64_t ComputeDigest(const ReproducibleBuildSpec& spec) const;
};

struct ProductionPluginManifest
{
    std::string name;
    std::string version;
    std::string abiVersion;
    std::string engineVersionRange;
    std::string binary;
    std::vector<std::string> dependencies;
    bool signedBinary{};
};

class URHO3D_API ProductionPluginRegistry
{
public:
    bool Add(const ProductionPluginManifest& manifest, std::string* error = nullptr);
    const ProductionPluginManifest* Find(const std::string& name) const;
    bool Validate(std::string* error = nullptr) const;
    bool Remove(const std::string& name);
    void Clear();
    const std::vector<ProductionPluginManifest>& GetManifests() const { return manifests_; }

private:
    std::vector<ProductionPluginManifest> manifests_;
};

struct DependencyAuditEntry
{
    std::string name;
    std::string version;
    std::string license;
    std::string sha256;
    std::uint64_t knownVulnerabilities{};
};

class URHO3D_API ProductionDependencyAudit
{
public:
    bool Add(const DependencyAuditEntry& entry);
    bool Validate(std::string* error = nullptr) const;
    std::uint64_t ComputeDigest() const;
    void Clear();
    const std::vector<DependencyAuditEntry>& GetEntries() const { return entries_; }

private:
    std::vector<DependencyAuditEntry> entries_;
};

struct ReferenceProjectEvidence
{
    std::string name;
    std::string projectPath;
    std::string artifactDigest;
    std::string platform;
    bool created{};
    bool opened{};
    bool edited{};
    bool saved{};
    bool packaged{};
    bool launched{};
};

class URHO3D_API ReferenceProjectCatalog
{
public:
    bool Add(const ReferenceProjectEvidence& evidence);
    bool ValidateDesktopCoverage(std::string* error = nullptr) const;
    const ReferenceProjectEvidence* Find(const std::string& name) const;
    void Clear();
    const std::vector<ReferenceProjectEvidence>& GetEntries() const { return entries_; }

private:
    std::vector<ReferenceProjectEvidence> entries_;
};

struct ProductionDocumentationEvidence
{
    bool installation{};
    bool firstProject{};
    bool editorManual{};
    bool apiReference{};
    bool assetPipeline{};
    bool networking{};
    bool plugins{};
    bool migration{};
    bool troubleshooting{};
    bool examples{};
};

class URHO3D_API ProductionDocumentationGate
{
public:
    bool Validate(const ProductionDocumentationEvidence& evidence, std::string* error = nullptr) const;
};

struct ReleasePackagingEvidence
{
    std::string version;
    std::string platform;
    std::string sourceCommit;
    std::string archiveSha256;
    bool symbolsSeparated{};
    bool checksumPublished{};
    bool changelogPublished{};
    bool rollbackDocumented{};
    bool signedArtifacts{};
};

class URHO3D_API ProductionReleaseGate
{
public:
    bool Validate(const ReleasePackagingEvidence& packaging, const ProductionDocumentationEvidence& documentation,
        std::string* error = nullptr) const;
};

struct ProductionReadinessInput
{
    SanitizerEvidence sanitizers;
    SoakBudget soakBudget;
    SoakEvidence soakEvidence;
    ProductionDocumentationEvidence documentation;
    ReleasePackagingEvidence packaging;
    ReproducibleBuildSpec reproducibleBuild;
    bool nativeDesktopMatrix{};
    bool referenceProjects{};
    bool dependencyAudit{};
    bool performanceBudgets{};
};

struct ProductionReadinessResult
{
    bool ready{};
    std::vector<std::string> passed;
    std::vector<std::string> blocked;
};

/// Aggregates independent production gates without overstating missing native evidence.
class URHO3D_API ProductionReadinessEvaluator
{
public:
    ProductionReadinessResult Evaluate(const ProductionReadinessInput& input) const;
};

} // namespace Urho3D

