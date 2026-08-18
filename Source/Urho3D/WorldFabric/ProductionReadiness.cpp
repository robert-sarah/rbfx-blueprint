// SPDX-License-Identifier: MIT
#include <Urho3D/WorldFabric/ProductionReadiness.h>

#include <algorithm>
#include <cmath>
#include <sstream>

namespace Urho3D
{
namespace
{

constexpr std::uint64_t FnvOffset = 1469598103934665603ull;
constexpr std::uint64_t FnvPrime = 1099511628211ull;

void HashText(std::uint64_t& digest, const std::string& text)
{
    for (const unsigned char character : text)
    {
        digest ^= character;
        digest *= FnvPrime;
    }
    digest ^= 0xffu;
    digest *= FnvPrime;
}

void HashBool(std::uint64_t& digest, bool value)
{
    digest ^= value ? 1u : 0u;
    digest *= FnvPrime;
}

bool IsFiniteNonNegative(double value)
{
    return std::isfinite(value) && value >= 0.0;
}

const char* PlatformName(ProductionPlatform platform)
{
    switch (platform)
    {
    case ProductionPlatform::Linux: return "Linux";
    case ProductionPlatform::Windows: return "Windows";
    case ProductionPlatform::macOS: return "macOS";
    case ProductionPlatform::Android: return "Android";
    case ProductionPlatform::iOS: return "iOS";
    case ProductionPlatform::Web: return "Web";
    }
    return "Unknown";
}

bool HasText(const std::string& value)
{
    return !value.empty();
}

} // namespace

ProductionDiagnosticLog::ProductionDiagnosticLog(std::size_t capacity)
    : capacity_(capacity)
{
}

std::string ProductionDiagnosticLog::RedactSensitive(const std::string& text)
{
    std::string redacted = text;
    const auto redact = [&redacted](const std::string& marker)
    {
        std::size_t position = 0;
        while ((position = redacted.find(marker, position)) != std::string::npos)
        {
            redacted.replace(position, marker.size(), "<user-path>");
            position += 11;
        }
    };
    redact("/home/");
    redact("/Users/");
    redact("C:\\\\Users\\\\");
    return redacted;
}

bool ProductionDiagnosticLog::Add(const ProductionDiagnostic& diagnostic)
{
    if (capacity_ == 0 || records_.size() >= capacity_ || diagnostic.code.empty() || diagnostic.message.empty())
        return false;
    ProductionDiagnostic sanitized = diagnostic;
    sanitized.source = RedactSensitive(sanitized.source);
    sanitized.message = RedactSensitive(sanitized.message);
    records_.push_back(std::move(sanitized));
    return true;
}

bool ProductionDiagnosticLog::HasErrors() const
{
    return std::any_of(records_.begin(), records_.end(), [](const ProductionDiagnostic& diagnostic)
    {
        return diagnostic.severity == ProductionDiagnosticSeverity::Error
            || diagnostic.severity == ProductionDiagnosticSeverity::Critical;
    });
}

bool ProductionDiagnosticLog::HasCritical() const
{
    return std::any_of(records_.begin(), records_.end(), [](const ProductionDiagnostic& diagnostic)
    {
        return diagnostic.severity == ProductionDiagnosticSeverity::Critical;
    });
}

std::size_t ProductionDiagnosticLog::Count(ProductionDiagnosticSeverity severity) const
{
    return static_cast<std::size_t>(std::count_if(records_.begin(), records_.end(), [severity](const ProductionDiagnostic& diagnostic)
    {
        return diagnostic.severity == severity;
    }));
}

std::string ProductionDiagnosticLog::Serialize() const
{
    std::ostringstream output;
    for (const ProductionDiagnostic& diagnostic : records_)
    {
        output << diagnostic.code << '|' << static_cast<int>(diagnostic.severity) << '|'
               << diagnostic.source << '|' << diagnostic.line << ':' << diagnostic.column << '|'
               << diagnostic.message << '\n';
    }
    return output.str();
}

void ProductionDiagnosticLog::Clear()
{
    records_.clear();
}

bool ProductionSanitizerGate::Validate(const SanitizerEvidence& evidence, std::string* error) const
{
    if (!evidence.address || !evidence.undefinedBehavior || !evidence.staticAnalysis || !evidence.fuzzing)
    {
        if (error)
            *error = "required sanitizer, static-analysis, or fuzzing evidence is missing";
        return false;
    }
    if (evidence.thread && evidence.leak && evidence.diagnostic.empty())
        return true;
    if (!evidence.diagnostic.empty())
    {
        if (error)
            *error = evidence.diagnostic;
        return false;
    }
    if (error)
        *error = "sanitizer evidence is incomplete or reports an unclassified failure";
    return false;
}

bool ProductionSoakGate::Validate(const SoakBudget& budget, const SoakEvidence& evidence, std::string* error) const
{
    if (budget.minimumFrames == 0 || !IsFiniteNonNegative(budget.minimumDurationSeconds)
        || !IsFiniteNonNegative(budget.maximumFrameMilliseconds) || budget.maximumMemoryMegabytes == 0)
    {
        if (error)
            *error = "invalid soak budget";
        return false;
    }
    if (evidence.frames < budget.minimumFrames || evidence.durationSeconds < budget.minimumDurationSeconds
        || evidence.maxFrameMilliseconds > budget.maximumFrameMilliseconds
        || evidence.maxMemoryMegabytes > budget.maximumMemoryMegabytes || evidence.crashCount != 0
        || evidence.dataRaceCount != 0 || evidence.invalidResourceCount != 0)
    {
        if (error)
            *error = "soak evidence does not satisfy the production budget";
        return false;
    }
    return true;
}

bool ProductionNativeReleaseGate::Add(const NativePlatformEvidence& evidence)
{
    if (Find(evidence.platform))
        return false;
    entries_.push_back(evidence);
    return true;
}

const NativePlatformEvidence* ProductionNativeReleaseGate::Find(ProductionPlatform platform) const
{
    const auto iterator = std::find_if(entries_.begin(), entries_.end(), [platform](const NativePlatformEvidence& entry)
    {
        return entry.platform == platform;
    });
    return iterator == entries_.end() ? nullptr : &*iterator;
}

bool ProductionNativeReleaseGate::ValidateDesktopMatrix(std::string* error) const
{
    const ProductionPlatform desktopPlatforms[] = {ProductionPlatform::Linux, ProductionPlatform::Windows, ProductionPlatform::macOS};
    for (const ProductionPlatform platform : desktopPlatforms)
    {
        const NativePlatformEvidence* entry = Find(platform);
        if (!entry || !entry->configured || !entry->compiled || !entry->testsExecuted || !entry->graphicalSmoke
            || !entry->installTested || !entry->packageVerified || entry->architecture.empty() || entry->compiler.empty()
            || entry->graphicsBackend.empty() || entry->artifactDigest.empty())
        {
            if (error)
                *error = std::string("incomplete native evidence for ") + PlatformName(platform);
            return false;
        }
    }
    return true;
}

void ProductionNativeReleaseGate::Clear()
{
    entries_.clear();
}

bool ReproducibleBuildVerifier::Validate(const ReproducibleBuildSpec& spec, std::string* error) const
{
    const std::string* required[] = {&spec.sourceCommit, &spec.compiler, &spec.compilerVersion, &spec.cmakeVersion,
        &spec.ninjaVersion, &spec.toolchain, &spec.buildType, &spec.dependencyDigest, &spec.sourceDigest, &spec.artifactDigest};
    for (const std::string* value : required)
    {
        if (!HasText(*value))
        {
            if (error)
                *error = "reproducible build specification is incomplete";
            return false;
        }
    }
    return true;
}

bool ReproducibleBuildVerifier::Equivalent(const ReproducibleBuildSpec& left, const ReproducibleBuildSpec& right) const
{
    return left.sourceCommit == right.sourceCommit && left.compiler == right.compiler
        && left.compilerVersion == right.compilerVersion && left.cmakeVersion == right.cmakeVersion
        && left.ninjaVersion == right.ninjaVersion && left.toolchain == right.toolchain
        && left.buildType == right.buildType && left.dependencyDigest == right.dependencyDigest
        && left.sourceDigest == right.sourceDigest && left.artifactDigest == right.artifactDigest;
}

std::uint64_t ReproducibleBuildVerifier::ComputeDigest(const ReproducibleBuildSpec& spec) const
{
    std::uint64_t digest = FnvOffset;
    HashText(digest, spec.sourceCommit);
    HashText(digest, spec.compiler);
    HashText(digest, spec.compilerVersion);
    HashText(digest, spec.cmakeVersion);
    HashText(digest, spec.ninjaVersion);
    HashText(digest, spec.toolchain);
    HashText(digest, spec.buildType);
    HashText(digest, spec.dependencyDigest);
    HashText(digest, spec.sourceDigest);
    HashText(digest, spec.artifactDigest);
    return digest;
}

bool ProductionPluginRegistry::Add(const ProductionPluginManifest& manifest, std::string* error)
{
    if (manifest.name.empty() || manifest.version.empty() || manifest.abiVersion.empty() || manifest.binary.empty())
    {
        if (error)
            *error = "plugin manifest is incomplete";
        return false;
    }
    if (Find(manifest.name))
    {
        if (error)
            *error = "plugin name is already registered";
        return false;
    }
    manifests_.push_back(manifest);
    return true;
}

const ProductionPluginManifest* ProductionPluginRegistry::Find(const std::string& name) const
{
    const auto iterator = std::find_if(manifests_.begin(), manifests_.end(), [&name](const ProductionPluginManifest& manifest)
    {
        return manifest.name == name;
    });
    return iterator == manifests_.end() ? nullptr : &*iterator;
}

bool ProductionPluginRegistry::Validate(std::string* error) const
{
    for (const ProductionPluginManifest& manifest : manifests_)
    {
        if (manifest.abiVersion != "1" || manifest.engineVersionRange.empty() || !manifest.signedBinary)
        {
            if (error)
                *error = "plugin registry contains an unsigned or incompatible plugin";
            return false;
        }
        for (const std::string& dependency : manifest.dependencies)
        {
            if (dependency == manifest.name)
            {
                if (error)
                    *error = "plugin cannot depend on itself";
                return false;
            }
        }
    }
    return true;
}

bool ProductionPluginRegistry::Remove(const std::string& name)
{
    const auto iterator = std::find_if(manifests_.begin(), manifests_.end(), [&name](const ProductionPluginManifest& manifest)
    {
        return manifest.name == name;
    });
    if (iterator == manifests_.end())
        return false;
    manifests_.erase(iterator);
    return true;
}

void ProductionPluginRegistry::Clear()
{
    manifests_.clear();
}

bool ProductionDependencyAudit::Add(const DependencyAuditEntry& entry)
{
    if (entry.name.empty() || entry.version.empty() || entry.license.empty() || entry.sha256.empty())
        return false;
    if (std::any_of(entries_.begin(), entries_.end(), [&entry](const DependencyAuditEntry& existing)
        { return existing.name == entry.name; }))
        return false;
    entries_.push_back(entry);
    return true;
}

bool ProductionDependencyAudit::Validate(std::string* error) const
{
    for (const DependencyAuditEntry& entry : entries_)
    {
        if (entry.knownVulnerabilities != 0)
        {
            if (error)
                *error = "dependency audit contains known vulnerabilities: " + entry.name;
            return false;
        }
    }
    return !entries_.empty();
}

std::uint64_t ProductionDependencyAudit::ComputeDigest() const
{
    std::vector<DependencyAuditEntry> sorted = entries_;
    std::sort(sorted.begin(), sorted.end(), [](const DependencyAuditEntry& left, const DependencyAuditEntry& right)
    {
        return left.name < right.name;
    });
    std::uint64_t digest = FnvOffset;
    for (const DependencyAuditEntry& entry : sorted)
    {
        HashText(digest, entry.name);
        HashText(digest, entry.version);
        HashText(digest, entry.license);
        HashText(digest, entry.sha256);
        HashBool(digest, entry.knownVulnerabilities != 0);
    }
    return digest;
}

void ProductionDependencyAudit::Clear()
{
    entries_.clear();
}

bool ReferenceProjectCatalog::Add(const ReferenceProjectEvidence& evidence)
{
    if (evidence.name.empty() || evidence.projectPath.empty() || evidence.artifactDigest.empty() || evidence.platform.empty())
        return false;
    if (Find(evidence.name))
        return false;
    entries_.push_back(evidence);
    return true;
}

bool ReferenceProjectCatalog::ValidateDesktopCoverage(std::string* error) const
{
    const char* requiredProjects[] = {"reference-2d", "reference-3d", "reference-hybrid"};
    for (const char* required : requiredProjects)
    {
        const ReferenceProjectEvidence* evidence = Find(required);
        if (!evidence || !evidence->created || !evidence->opened || !evidence->edited || !evidence->saved
            || !evidence->packaged || !evidence->launched)
        {
            if (error)
                *error = std::string("incomplete reference project: ") + required;
            return false;
        }
    }
    return true;
}

const ReferenceProjectEvidence* ReferenceProjectCatalog::Find(const std::string& name) const
{
    const auto iterator = std::find_if(entries_.begin(), entries_.end(), [&name](const ReferenceProjectEvidence& evidence)
    {
        return evidence.name == name;
    });
    return iterator == entries_.end() ? nullptr : &*iterator;
}

void ReferenceProjectCatalog::Clear()
{
    entries_.clear();
}

bool ProductionDocumentationGate::Validate(const ProductionDocumentationEvidence& evidence, std::string* error) const
{
    const bool complete = evidence.installation && evidence.firstProject && evidence.editorManual && evidence.apiReference
        && evidence.assetPipeline && evidence.networking && evidence.plugins && evidence.migration
        && evidence.troubleshooting && evidence.examples;
    if (!complete && error)
        *error = "production documentation is incomplete";
    return complete;
}

bool ProductionReleaseGate::Validate(const ReleasePackagingEvidence& packaging,
    const ProductionDocumentationEvidence& documentation, std::string* error) const
{
    ProductionDocumentationGate documentationGate;
    if (!documentationGate.Validate(documentation, error))
        return false;
    if (packaging.version.empty() || packaging.platform.empty() || packaging.sourceCommit.empty()
        || packaging.archiveSha256.empty() || !packaging.symbolsSeparated || !packaging.checksumPublished
        || !packaging.changelogPublished || !packaging.rollbackDocumented || !packaging.signedArtifacts)
    {
        if (error)
            *error = "release packaging evidence is incomplete";
        return false;
    }
    return true;
}

ProductionReadinessResult ProductionReadinessEvaluator::Evaluate(const ProductionReadinessInput& input) const
{
    ProductionReadinessResult result;
    std::string error;
    ProductionSanitizerGate sanitizerGate;
    if (sanitizerGate.Validate(input.sanitizers, &error))
        result.passed.push_back("sanitizers-static-analysis-fuzzing");
    else
        result.blocked.push_back("sanitizers: " + error);

    ProductionSoakGate soakGate;
    if (soakGate.Validate(input.soakBudget, input.soakEvidence, &error))
        result.passed.push_back("long-run-soak");
    else
        result.blocked.push_back("soak: " + error);

    if (input.nativeDesktopMatrix)
        result.passed.push_back("native-desktop-matrix");
    else
        result.blocked.push_back("native desktop matrix is incomplete");

    ReproducibleBuildVerifier buildVerifier;
    if (buildVerifier.Validate(input.reproducibleBuild, &error))
        result.passed.push_back("reproducible-build");
    else
        result.blocked.push_back("reproducible build: " + error);

    if (input.referenceProjects)
        result.passed.push_back("reference-projects");
    else
        result.blocked.push_back("reference projects are incomplete");
    if (input.dependencyAudit)
        result.passed.push_back("dependency-audit");
    else
        result.blocked.push_back("dependency audit is incomplete");
    if (input.performanceBudgets)
        result.passed.push_back("performance-budgets");
    else
        result.blocked.push_back("performance budgets are incomplete");

    ProductionReleaseGate releaseGate;
    if (releaseGate.Validate(input.packaging, input.documentation, &error))
        result.passed.push_back("documentation-and-release");
    else
        result.blocked.push_back("documentation/release: " + error);

    result.ready = result.blocked.empty();
    return result;
}

} // namespace Urho3D

