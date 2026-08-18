// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT
#pragma once

#include <Urho3D/Urho3D.h>

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace Urho3D
{

/// Versioned marker for the final production contracts documented by the engine.
constexpr const char* ProductionEngineVersion = "0.7.0-production";

enum class ProductionPlatform
{
    Linux,
    Windows,
    macOS,
    Android,
    iOS,
    Web
};

struct NativeValidationEntry
{
    ProductionPlatform platform{ProductionPlatform::Linux};
    bool configured{};
    bool compiled{};
    bool testsExecuted{};
    bool graphicalSmoke{};
    std::string diagnostic;
};

/// Records native validation evidence without claiming that an unexecuted stage passed.
class URHO3D_API NativeValidationMatrix
{
public:
    void Clear();
    bool Add(const NativeValidationEntry& entry);
    const NativeValidationEntry* Find(ProductionPlatform platform) const;
    bool Validate(std::string* error = nullptr) const;
    std::uint64_t ComputeDigest() const;
    const std::vector<NativeValidationEntry>& GetEntries() const { return entries_; }

private:
    std::vector<NativeValidationEntry> entries_;
};

struct LongRunPlan
{
    std::uint64_t frames{};
    double fixedDeltaSeconds{1.0 / 60.0};
    std::uint64_t checkpointInterval{60};
};

struct LongRunResult
{
    bool success{};
    std::uint64_t framesExecuted{};
    std::uint64_t checkpoints{};
    double simulatedSeconds{};
    std::uint64_t digest{};
    std::string error;
};

using LongRunStep = std::function<bool(std::uint64_t frame, double deltaSeconds)>;

/// Executes deterministic, bounded soak plans suitable for CI and local endurance tests.
class URHO3D_API LongRunSoakRunner
{
public:
    LongRunResult Run(const LongRunPlan& plan, const LongRunStep& step) const;
};

struct PerformanceBudget
{
    double maxCpuMilliseconds{};
    double maxGpuMilliseconds{};
    double maxFrameMilliseconds{};
    std::uint64_t maxMemoryMegabytes{};
};

struct PerformanceSample
{
    double cpuMilliseconds{};
    double gpuMilliseconds{};
    double frameMilliseconds{};
    std::uint64_t memoryMegabytes{};
};

class URHO3D_API PerformanceBudgetGate
{
public:
    bool Evaluate(const PerformanceBudget& budget, const PerformanceSample& sample, std::string* error = nullptr) const;
};

/// Stores a bounded CPU/GPU frame history and exposes deterministic summary statistics.
class URHO3D_API FrameMetricsRecorder
{
public:
    explicit FrameMetricsRecorder(std::size_t capacity = 240);
    void SetCapacity(std::size_t capacity);
    void Record(const PerformanceSample& sample);
    void Clear();
    const std::vector<PerformanceSample>& GetSamples() const { return samples_; }
    double GetAverageFrameMilliseconds() const;
    double GetP95FrameMilliseconds() const;

private:
    std::size_t capacity_{};
    std::vector<PerformanceSample> samples_;
};

struct AnimationRetargetProfile
{
    std::string sourceRig;
    std::string targetRig;
    std::map<std::string, std::string> boneMap;
};

class URHO3D_API ProductionAnimationRetargeter
{
public:
    bool Validate(const AnimationRetargetProfile& profile, std::string* error = nullptr) const;
    std::size_t GetMappedBoneCount(const AnimationRetargetProfile& profile) const;
};

struct AnimationBlendNode
{
    std::string name;
    float weight{};
};

class URHO3D_API AnimationBlendGraph
{
public:
    bool AddNode(const AnimationBlendNode& node);
    void Clear();
    std::vector<AnimationBlendNode> EvaluateNormalized() const;

private:
    std::vector<AnimationBlendNode> nodes_;
};

struct CinematicKeyframe
{
    double timeSeconds{};
    std::string property;
    float value{};
};

class URHO3D_API CinematicTimeline
{
public:
    bool AddKeyframe(const CinematicKeyframe& keyframe);
    float Evaluate(const std::string& property, double timeSeconds, float defaultValue = 0.0f) const;
    const std::vector<CinematicKeyframe>& GetKeyframes() const { return keyframes_; }

private:
    std::vector<CinematicKeyframe> keyframes_;
};

struct CinematicShot
{
    std::string id;
    double startSeconds{};
    double endSeconds{};
    std::string camera;
};

class URHO3D_API CinematicShotList
{
public:
    bool AddShot(const CinematicShot& shot, std::string* error = nullptr);
    const CinematicShot* FindActive(double timeSeconds) const;
    const std::vector<CinematicShot>& GetShots() const { return shots_; }

private:
    std::vector<CinematicShot> shots_;
};

enum class ProductionAssetKind
{
    Texture,
    Model2D,
    Model3D,
    Animation,
    Material,
    Audio,
    Scene,
    Unknown
};

struct AssetImportProfile
{
    std::string sourcePath;
    ProductionAssetKind kind{ProductionAssetKind::Unknown};
    bool optimize{};
    bool generateLod{};
    bool generateCollision{};
};

struct ProductionAssetImportResult
{
    bool accepted{};
    std::string normalizedExtension;
    std::string artifactPath;
    std::string error;
};

class URHO3D_API AssetImportValidator
{
public:
    bool Validate(const AssetImportProfile& profile, ProductionAssetImportResult& result) const;
};

class URHO3D_API AssetDependencyManifest
{
public:
    bool AddAsset(const std::string& asset, const std::vector<std::string>& dependencies);
    bool ValidateAcyclic(std::string* error = nullptr) const;
    std::vector<std::string> GetTransitiveDependencies(const std::string& asset) const;
    std::uint64_t ComputeDigest() const;

private:
    std::map<std::string, std::vector<std::string>> dependencies_;
};

struct AssetExportManifest
{
    std::string target;
    std::vector<std::string> artifacts;
    std::uint64_t sourceDigest{};
};

class URHO3D_API AssetExportPlanner
{
public:
    bool Validate(const AssetExportManifest& manifest, std::string* error = nullptr) const;
    std::uint64_t ComputeDigest(const AssetExportManifest& manifest) const;
};

struct UIStyle
{
    std::string name;
    float padding{};
    float cornerRadius{};
    float opacity{1.0f};
};

class URHO3D_API UIStyleSheetRuntime
{
public:
    bool SetStyle(const UIStyle& style);
    const UIStyle* FindStyle(const std::string& name) const;
    void Clear();

private:
    std::map<std::string, UIStyle> styles_;
};

struct UIWidgetLayoutRequest
{
    float parentWidth{};
    float parentHeight{};
    float anchorMinX{};
    float anchorMinY{};
    float anchorMaxX{};
    float anchorMaxY{};
    float offsetLeft{};
    float offsetTop{};
    float offsetRight{};
    float offsetBottom{};
};

struct UIWidgetRect
{
    float left{};
    float top{};
    float right{};
    float bottom{};
};

class URHO3D_API UIWidgetLayoutEngine
{
public:
    UIWidgetRect Resolve(const UIWidgetLayoutRequest& request) const;
};

struct ProductionPackageManifest
{
    std::string name;
    std::string version;
    ProductionPlatform platform{ProductionPlatform::Linux};
    std::vector<std::string> artifacts;
    std::uint64_t contentDigest{};
};

class URHO3D_API PackageManifestBuilder
{
public:
    bool AddArtifact(ProductionPackageManifest& manifest, const std::string& artifact) const;
    bool Validate(const ProductionPackageManifest& manifest, std::string* error = nullptr) const;
    std::uint64_t ComputeDigest(const ProductionPackageManifest& manifest) const;
};

class URHO3D_API PackageTargetMatrix
{
public:
    bool Add(ProductionPlatform platform);
    bool Has(ProductionPlatform platform) const;
    bool ValidateDesktopCoverage(std::string* error = nullptr) const;

private:
    std::vector<ProductionPlatform> platforms_;
};

struct CrashRecoverySnapshot
{
    std::uint64_t sequence{};
    std::string scenePath;
    std::string worldState;
};

class URHO3D_API CrashRecoveryJournal
{
public:
    bool Append(const CrashRecoverySnapshot& snapshot);
    const CrashRecoverySnapshot* GetLatest() const;
    bool RestoreLatest(CrashRecoverySnapshot& output) const;
    void Clear();

private:
    std::vector<CrashRecoverySnapshot> snapshots_;
};

class URHO3D_API InputActionMapRuntime
{
public:
    bool Bind(const std::string& action, const std::string& input);
    std::string Resolve(const std::string& input) const;
    void Clear();

private:
    std::map<std::string, std::string> bindings_;
};

struct ScreenshotRegressionCase
{
    std::string name;
    std::uint64_t expectedDigest{};
};

class URHO3D_API ScreenshotRegressionCatalog
{
public:
    bool Add(const ScreenshotRegressionCase& test);
    bool Compare(const std::string& name, std::uint64_t actualDigest, std::string* error = nullptr) const;
    void Clear();

private:
    std::map<std::string, std::uint64_t> expected_;
};

struct HotReloadResourceRecord
{
    std::string resourceId;
    std::uint64_t previousDigest{};
    std::uint64_t currentDigest{};
    bool stateMigrated{};
};

/// Tracks resource reloads and requires explicit state migration evidence.
class URHO3D_API ResourceHotReloadCoordinator
{
public:
    bool Register(const HotReloadResourceRecord& record);
    bool Apply(const std::string& resourceId, bool stateMigrated);
    const HotReloadResourceRecord* Find(const std::string& resourceId) const;
    const std::vector<HotReloadResourceRecord>& GetRecords() const { return records_; }
    void Clear();

private:
    std::vector<HotReloadResourceRecord> records_;
};

struct BuildProvenanceRecord
{
    std::string step;
    std::string toolchain;
    std::uint64_t inputDigest{};
    std::uint64_t outputDigest{};
};

/// Stores reproducible build evidence and rejects duplicate or incomplete steps.
class URHO3D_API BuildProvenanceLedger
{
public:
    bool Add(const BuildProvenanceRecord& record);
    bool Validate(std::string* error = nullptr) const;
    std::uint64_t ComputeDigest() const;
    const std::vector<BuildProvenanceRecord>& GetRecords() const { return records_; }
    void Clear();

private:
    std::vector<BuildProvenanceRecord> records_;
};

} // namespace Urho3D
