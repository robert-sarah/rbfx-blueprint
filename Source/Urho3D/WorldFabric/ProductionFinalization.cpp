// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT
#include <Urho3D/WorldFabric/ProductionFinalization.h>

#include <algorithm>
#include <cmath>
#include <cctype>
#include <limits>
#include <set>

namespace Urho3D
{
namespace
{

std::uint64_t HashBytes(std::uint64_t hash, const std::string& value)
{
    for (const unsigned char character : value)
    {
        hash ^= character;
        hash *= 1099511628211ull;
    }
    return hash;
}

std::uint64_t HashNumber(std::uint64_t hash, std::uint64_t value)
{
    for (unsigned i = 0; i < 8; ++i)
    {
        hash ^= static_cast<unsigned char>(value & 0xffu);
        hash *= 1099511628211ull;
        value >>= 8u;
    }
    return hash;
}

std::string LowerExtension(const std::string& path)
{
    const std::size_t dot = path.find_last_of('.');
    if (dot == std::string::npos || dot + 1 >= path.size())
        return {};

    std::string extension = path.substr(dot + 1);
    std::transform(extension.begin(), extension.end(), extension.begin(),
        [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return extension;
}

const char* PlatformName(ProductionPlatform platform)
{
    switch (platform)
    {
    case ProductionPlatform::Linux: return "linux";
    case ProductionPlatform::Windows: return "windows";
    case ProductionPlatform::macOS: return "macos";
    case ProductionPlatform::Android: return "android";
    case ProductionPlatform::iOS: return "ios";
    case ProductionPlatform::Web: return "web";
    }
    return "unknown";
}

bool IsFinitePositive(double value)
{
    return std::isfinite(value) && value > 0.0;
}

} // namespace

void NativeValidationMatrix::Clear()
{
    entries_.clear();
}

bool NativeValidationMatrix::Add(const NativeValidationEntry& entry)
{
    if (Find(entry.platform))
        return false;
    entries_.push_back(entry);
    return true;
}

const NativeValidationEntry* NativeValidationMatrix::Find(ProductionPlatform platform) const
{
    const auto it = std::find_if(entries_.begin(), entries_.end(),
        [platform](const NativeValidationEntry& entry) { return entry.platform == platform; });
    return it == entries_.end() ? nullptr : &*it;
}

bool NativeValidationMatrix::Validate(std::string* error) const
{
    for (const NativeValidationEntry& entry : entries_)
    {
        if (!entry.configured && (entry.compiled || entry.testsExecuted || entry.graphicalSmoke))
        {
            if (error)
                *error = std::string(PlatformName(entry.platform)) + " reports evidence without configuration";
            return false;
        }
        if (!entry.compiled && (entry.testsExecuted || entry.graphicalSmoke))
        {
            if (error)
                *error = std::string(PlatformName(entry.platform)) + " reports execution without compilation";
            return false;
        }
        if (entry.graphicalSmoke && !entry.testsExecuted)
        {
            if (error)
                *error = std::string(PlatformName(entry.platform)) + " reports graphical smoke without tests";
            return false;
        }
    }
    return true;
}

std::uint64_t NativeValidationMatrix::ComputeDigest() const
{
    std::vector<NativeValidationEntry> sorted = entries_;
    std::sort(sorted.begin(), sorted.end(), [](const NativeValidationEntry& left, const NativeValidationEntry& right)
    {
        return static_cast<int>(left.platform) < static_cast<int>(right.platform);
    });

    std::uint64_t digest = 1469598103934665603ull;
    for (const NativeValidationEntry& entry : sorted)
    {
        digest = HashNumber(digest, static_cast<unsigned>(entry.platform));
        digest = HashNumber(digest, entry.configured);
        digest = HashNumber(digest, entry.compiled);
        digest = HashNumber(digest, entry.testsExecuted);
        digest = HashNumber(digest, entry.graphicalSmoke);
        digest = HashBytes(digest, entry.diagnostic);
    }
    return digest;
}

LongRunResult LongRunSoakRunner::Run(const LongRunPlan& plan, const LongRunStep& step) const
{
    LongRunResult result;
    if (plan.frames == 0 || !IsFinitePositive(plan.fixedDeltaSeconds) || plan.checkpointInterval == 0 || !step)
    {
        result.error = "invalid long-run plan";
        return result;
    }

    result.digest = 1469598103934665603ull;
    for (std::uint64_t frame = 0; frame < plan.frames; ++frame)
    {
        if (!step(frame, plan.fixedDeltaSeconds))
        {
            result.error = "soak step failed at frame " + std::to_string(frame);
            result.framesExecuted = frame;
            return result;
        }
        result.digest = HashNumber(result.digest, frame);
        if ((frame + 1) % plan.checkpointInterval == 0)
        {
            ++result.checkpoints;
            result.digest = HashNumber(result.digest, result.checkpoints);
        }
        ++result.framesExecuted;
    }
    result.simulatedSeconds = static_cast<double>(result.framesExecuted) * plan.fixedDeltaSeconds;
    result.success = true;
    return result;
}

bool PerformanceBudgetGate::Evaluate(const PerformanceBudget& budget, const PerformanceSample& sample, std::string* error) const
{
    if (!std::isfinite(sample.cpuMilliseconds) || !std::isfinite(sample.gpuMilliseconds)
        || !std::isfinite(sample.frameMilliseconds) || sample.cpuMilliseconds < 0.0
        || sample.gpuMilliseconds < 0.0 || sample.frameMilliseconds < 0.0)
    {
        if (error)
            *error = "performance sample contains an invalid duration";
        return false;
    }
    if (budget.maxCpuMilliseconds > 0.0 && sample.cpuMilliseconds > budget.maxCpuMilliseconds)
    {
        if (error)
            *error = "CPU budget exceeded";
        return false;
    }
    if (budget.maxGpuMilliseconds > 0.0 && sample.gpuMilliseconds > budget.maxGpuMilliseconds)
    {
        if (error)
            *error = "GPU budget exceeded";
        return false;
    }
    if (budget.maxFrameMilliseconds > 0.0 && sample.frameMilliseconds > budget.maxFrameMilliseconds)
    {
        if (error)
            *error = "frame budget exceeded";
        return false;
    }
    if (budget.maxMemoryMegabytes > 0 && sample.memoryMegabytes > budget.maxMemoryMegabytes)
    {
        if (error)
            *error = "memory budget exceeded";
        return false;
    }
    return true;
}

FrameMetricsRecorder::FrameMetricsRecorder(std::size_t capacity)
    : capacity_(capacity)
{
}

void FrameMetricsRecorder::SetCapacity(std::size_t capacity)
{
    capacity_ = capacity;
    if (capacity_ == 0)
    {
        samples_.clear();
        return;
    }
    if (samples_.size() > capacity_)
        samples_.erase(samples_.begin(), samples_.end() - static_cast<std::ptrdiff_t>(capacity_));
}

void FrameMetricsRecorder::Record(const PerformanceSample& sample)
{
    if (capacity_ == 0)
        return;
    samples_.push_back(sample);
    if (samples_.size() > capacity_)
        samples_.erase(samples_.begin());
}

void FrameMetricsRecorder::Clear()
{
    samples_.clear();
}

double FrameMetricsRecorder::GetAverageFrameMilliseconds() const
{
    if (samples_.empty())
        return 0.0;
    double total = 0.0;
    for (const PerformanceSample& sample : samples_)
        total += sample.frameMilliseconds;
    return total / static_cast<double>(samples_.size());
}

double FrameMetricsRecorder::GetP95FrameMilliseconds() const
{
    if (samples_.empty())
        return 0.0;
    std::vector<double> values;
    values.reserve(samples_.size());
    for (const PerformanceSample& sample : samples_)
        values.push_back(sample.frameMilliseconds);
    std::sort(values.begin(), values.end());
    const std::size_t index = std::min(values.size() - 1,
        static_cast<std::size_t>(std::ceil(static_cast<double>(values.size()) * 0.95) - 1.0));
    return values[index];
}

bool AnimationRetargeter::Validate(const AnimationRetargetProfile& profile, std::string* error) const
{
    if (profile.sourceRig.empty() || profile.targetRig.empty() || profile.sourceRig == profile.targetRig)
    {
        if (error)
            *error = "source and target rigs must be distinct";
        return false;
    }
    std::set<std::string> targets;
    for (const auto& mapping : profile.boneMap)
    {
        if (mapping.first.empty() || mapping.second.empty() || !targets.insert(mapping.second).second)
        {
            if (error)
                *error = "retarget bone map contains an empty or duplicate target";
            return false;
        }
    }
    return !profile.boneMap.empty();
}

std::size_t AnimationRetargeter::GetMappedBoneCount(const AnimationRetargetProfile& profile) const
{
    return profile.boneMap.size();
}

bool AnimationBlendGraph::AddNode(const AnimationBlendNode& node)
{
    if (node.name.empty() || !std::isfinite(node.weight) || node.weight < 0.0f)
        return false;
    const auto duplicate = std::find_if(nodes_.begin(), nodes_.end(),
        [&node](const AnimationBlendNode& current) { return current.name == node.name; });
    if (duplicate != nodes_.end())
        return false;
    nodes_.push_back(node);
    return true;
}

void AnimationBlendGraph::Clear()
{
    nodes_.clear();
}

std::vector<AnimationBlendNode> AnimationBlendGraph::EvaluateNormalized() const
{
    std::vector<AnimationBlendNode> result = nodes_;
    if (result.empty())
        return result;
    float total = 0.0f;
    for (const AnimationBlendNode& node : result)
        total += node.weight;
    if (total <= std::numeric_limits<float>::epsilon())
    {
        const float equalWeight = 1.0f / static_cast<float>(result.size());
        for (AnimationBlendNode& node : result)
            node.weight = equalWeight;
    }
    else
    {
        for (AnimationBlendNode& node : result)
            node.weight /= total;
    }
    return result;
}

bool CinematicTimeline::AddKeyframe(const CinematicKeyframe& keyframe)
{
    if (keyframe.property.empty() || !std::isfinite(keyframe.timeSeconds) || keyframe.timeSeconds < 0.0
        || !std::isfinite(keyframe.value))
        return false;
    keyframes_.push_back(keyframe);
    std::stable_sort(keyframes_.begin(), keyframes_.end(), [](const CinematicKeyframe& left, const CinematicKeyframe& right)
    {
        if (left.property != right.property)
            return left.property < right.property;
        return left.timeSeconds < right.timeSeconds;
    });
    return true;
}

float CinematicTimeline::Evaluate(const std::string& property, double timeSeconds, float defaultValue) const
{
    if (property.empty() || !std::isfinite(timeSeconds))
        return defaultValue;
    const CinematicKeyframe* previous = nullptr;
    const CinematicKeyframe* next = nullptr;
    for (const CinematicKeyframe& keyframe : keyframes_)
    {
        if (keyframe.property != property)
            continue;
        if (keyframe.timeSeconds <= timeSeconds)
            previous = &keyframe;
        if (keyframe.timeSeconds >= timeSeconds)
        {
            next = &keyframe;
            break;
        }
    }
    if (!previous)
        return next ? next->value : defaultValue;
    if (!next || previous == next || next->timeSeconds <= previous->timeSeconds)
        return previous->value;
    const float factor = static_cast<float>((timeSeconds - previous->timeSeconds)
        / (next->timeSeconds - previous->timeSeconds));
    return previous->value + (next->value - previous->value) * factor;
}

bool CinematicShotList::AddShot(const CinematicShot& shot, std::string* error)
{
    if (shot.id.empty() || shot.camera.empty() || !std::isfinite(shot.startSeconds)
        || !std::isfinite(shot.endSeconds) || shot.startSeconds < 0.0 || shot.endSeconds <= shot.startSeconds)
    {
        if (error)
            *error = "invalid cinematic shot";
        return false;
    }
    for (const CinematicShot& existing : shots_)
    {
        if (existing.id == shot.id || (shot.startSeconds < existing.endSeconds && existing.startSeconds < shot.endSeconds))
        {
            if (error)
                *error = "cinematic shots overlap or reuse an id";
            return false;
        }
    }
    shots_.push_back(shot);
    std::sort(shots_.begin(), shots_.end(), [](const CinematicShot& left, const CinematicShot& right)
    {
        return left.startSeconds < right.startSeconds;
    });
    return true;
}

const CinematicShot* CinematicShotList::FindActive(double timeSeconds) const
{
    for (const CinematicShot& shot : shots_)
    {
        if (timeSeconds >= shot.startSeconds && timeSeconds < shot.endSeconds)
            return &shot;
    }
    return nullptr;
}

bool AssetImportValidator::Validate(const AssetImportProfile& profile, ProductionAssetImportResult& result) const
{
    result = {};
    result.normalizedExtension = LowerExtension(profile.sourcePath);
    if (profile.sourcePath.empty() || result.normalizedExtension.empty())
    {
        result.error = "asset source path has no extension";
        return false;
    }

    bool accepted = false;
    switch (profile.kind)
    {
    case ProductionAssetKind::Texture:
        accepted = result.normalizedExtension == "png" || result.normalizedExtension == "jpg"
            || result.normalizedExtension == "jpeg" || result.normalizedExtension == "tga"
            || result.normalizedExtension == "webp";
        break;
    case ProductionAssetKind::Model2D:
        accepted = result.normalizedExtension == "svg" || result.normalizedExtension == "aseprite"
            || result.normalizedExtension == "tmx";
        break;
    case ProductionAssetKind::Model3D:
        accepted = result.normalizedExtension == "gltf" || result.normalizedExtension == "glb"
            || result.normalizedExtension == "obj" || result.normalizedExtension == "fbx"
            || result.normalizedExtension == "dae" || result.normalizedExtension == "ply"
            || result.normalizedExtension == "stl";
        break;
    case ProductionAssetKind::Animation:
        accepted = result.normalizedExtension == "anim" || result.normalizedExtension == "bvh"
            || result.normalizedExtension == "fbx" || result.normalizedExtension == "gltf"
            || result.normalizedExtension == "glb";
        break;
    case ProductionAssetKind::Material:
        accepted = result.normalizedExtension == "material" || result.normalizedExtension == "mat";
        break;
    case ProductionAssetKind::Audio:
        accepted = result.normalizedExtension == "wav" || result.normalizedExtension == "ogg"
            || result.normalizedExtension == "mp3" || result.normalizedExtension == "flac";
        break;
    case ProductionAssetKind::Scene:
        accepted = result.normalizedExtension == "scene" || result.normalizedExtension == "xml"
            || result.normalizedExtension == "json";
        break;
    case ProductionAssetKind::Unknown:
        break;
    }
    if (!accepted)
    {
        result.error = "extension is incompatible with the selected asset kind";
        return false;
    }
    result.accepted = true;
    result.artifactPath = profile.sourcePath + (profile.optimize ? ".optimized.rbfx" : ".rbfx");
    return true;
}

bool AssetDependencyManifest::AddAsset(const std::string& asset, const std::vector<std::string>& dependencies)
{
    if (asset.empty() || dependencies_.find(asset) != dependencies_.end())
        return false;
    std::vector<std::string> normalized = dependencies;
    if (std::any_of(normalized.begin(), normalized.end(), [&asset](const std::string& dependency)
        { return dependency.empty() || dependency == asset; }))
        return false;
    std::sort(normalized.begin(), normalized.end());
    normalized.erase(std::unique(normalized.begin(), normalized.end()), normalized.end());
    dependencies_.emplace(asset, std::move(normalized));
    return true;
}

bool AssetDependencyManifest::ValidateAcyclic(std::string* error) const
{
    std::map<std::string, unsigned char> marks;
    std::function<bool(const std::string&)> visit = [&](const std::string& asset)
    {
        const unsigned char mark = marks[asset];
        if (mark == 1)
        {
            if (error)
                *error = "asset dependency cycle at " + asset;
            return false;
        }
        if (mark == 2)
            return true;
        marks[asset] = 1;
        const auto it = dependencies_.find(asset);
        if (it != dependencies_.end())
        {
            for (const std::string& dependency : it->second)
            {
                if (dependencies_.find(dependency) != dependencies_.end() && !visit(dependency))
                    return false;
            }
        }
        marks[asset] = 2;
        return true;
    };

    for (const auto& entry : dependencies_)
    {
        if (!visit(entry.first))
            return false;
    }
    return true;
}

std::vector<std::string> AssetDependencyManifest::GetTransitiveDependencies(const std::string& asset) const
{
    std::set<std::string> visited;
    std::function<void(const std::string&)> visit = [&](const std::string& current)
    {
        const auto it = dependencies_.find(current);
        if (it == dependencies_.end())
            return;
        for (const std::string& dependency : it->second)
        {
            if (visited.insert(dependency).second)
                visit(dependency);
        }
    };
    visit(asset);
    return {visited.begin(), visited.end()};
}

std::uint64_t AssetDependencyManifest::ComputeDigest() const
{
    std::uint64_t digest = 1469598103934665603ull;
    for (const auto& entry : dependencies_)
    {
        digest = HashBytes(digest, entry.first);
        for (const std::string& dependency : entry.second)
            digest = HashBytes(digest, dependency);
    }
    return digest;
}

bool AssetExportPlanner::Validate(const AssetExportManifest& manifest, std::string* error) const
{
    if (manifest.target.empty() || manifest.artifacts.empty() || manifest.sourceDigest == 0)
    {
        if (error)
            *error = "export manifest requires a target, artifacts and source digest";
        return false;
    }
    std::set<std::string> unique;
    for (const std::string& artifact : manifest.artifacts)
    {
        if (artifact.empty() || !unique.insert(artifact).second)
        {
            if (error)
                *error = "export manifest contains an empty or duplicate artifact";
            return false;
        }
    }
    return true;
}

std::uint64_t AssetExportPlanner::ComputeDigest(const AssetExportManifest& manifest) const
{
    std::vector<std::string> artifacts = manifest.artifacts;
    std::sort(artifacts.begin(), artifacts.end());
    std::uint64_t digest = HashBytes(1469598103934665603ull, manifest.target);
    digest = HashNumber(digest, manifest.sourceDigest);
    for (const std::string& artifact : artifacts)
        digest = HashBytes(digest, artifact);
    return digest;
}

bool UIStyleSheetRuntime::SetStyle(const UIStyle& style)
{
    if (style.name.empty() || !std::isfinite(style.padding) || !std::isfinite(style.cornerRadius)
        || !std::isfinite(style.opacity) || style.padding < 0.0f || style.cornerRadius < 0.0f
        || style.opacity < 0.0f || style.opacity > 1.0f)
        return false;
    styles_[style.name] = style;
    return true;
}

const UIStyle* UIStyleSheetRuntime::FindStyle(const std::string& name) const
{
    const auto it = styles_.find(name);
    return it == styles_.end() ? nullptr : &it->second;
}

void UIStyleSheetRuntime::Clear()
{
    styles_.clear();
}

UIWidgetRect UIWidgetLayoutEngine::Resolve(const UIWidgetLayoutRequest& request) const
{
    const float width = std::max(0.0f, request.parentWidth);
    const float height = std::max(0.0f, request.parentHeight);
    UIWidgetRect result;
    result.left = request.anchorMinX * width + request.offsetLeft;
    result.top = request.anchorMinY * height + request.offsetTop;
    result.right = request.anchorMaxX * width + request.offsetRight;
    result.bottom = request.anchorMaxY * height + request.offsetBottom;
    if (result.right < result.left)
        std::swap(result.left, result.right);
    if (result.bottom < result.top)
        std::swap(result.top, result.bottom);
    return result;
}

bool PackageManifestBuilder::AddArtifact(ProductionPackageManifest& manifest, const std::string& artifact) const
{
    if (artifact.empty() || std::find(manifest.artifacts.begin(), manifest.artifacts.end(), artifact) != manifest.artifacts.end())
        return false;
    manifest.artifacts.push_back(artifact);
    std::sort(manifest.artifacts.begin(), manifest.artifacts.end());
    return true;
}

bool PackageManifestBuilder::Validate(const ProductionPackageManifest& manifest, std::string* error) const
{
    if (manifest.name.empty() || manifest.version.empty() || manifest.artifacts.empty() || manifest.contentDigest == 0)
    {
        if (error)
            *error = "package manifest is incomplete";
        return false;
    }
    return true;
}

std::uint64_t PackageManifestBuilder::ComputeDigest(const ProductionPackageManifest& manifest) const
{
    std::uint64_t digest = HashBytes(1469598103934665603ull, manifest.name);
    digest = HashBytes(digest, manifest.version);
    digest = HashBytes(digest, PlatformName(manifest.platform));
    digest = HashNumber(digest, manifest.contentDigest);
    for (const std::string& artifact : manifest.artifacts)
        digest = HashBytes(digest, artifact);
    return digest;
}

bool PackageTargetMatrix::Add(ProductionPlatform platform)
{
    if (Has(platform))
        return false;
    platforms_.push_back(platform);
    return true;
}

bool PackageTargetMatrix::Has(ProductionPlatform platform) const
{
    return std::find(platforms_.begin(), platforms_.end(), platform) != platforms_.end();
}

bool PackageTargetMatrix::ValidateDesktopCoverage(std::string* error) const
{
    if (!Has(ProductionPlatform::Linux) || !Has(ProductionPlatform::Windows) || !Has(ProductionPlatform::macOS))
    {
        if (error)
            *error = "desktop package matrix requires Linux, Windows and macOS";
        return false;
    }
    return true;
}

bool CrashRecoveryJournal::Append(const CrashRecoverySnapshot& snapshot)
{
    if (snapshot.sequence == 0 || snapshot.scenePath.empty() || snapshot.worldState.empty())
        return false;
    if (!snapshots_.empty() && snapshot.sequence <= snapshots_.back().sequence)
        return false;
    snapshots_.push_back(snapshot);
    return true;
}

const CrashRecoverySnapshot* CrashRecoveryJournal::GetLatest() const
{
    return snapshots_.empty() ? nullptr : &snapshots_.back();
}

bool CrashRecoveryJournal::RestoreLatest(CrashRecoverySnapshot& output) const
{
    const CrashRecoverySnapshot* latest = GetLatest();
    if (!latest)
        return false;
    output = *latest;
    return true;
}

void CrashRecoveryJournal::Clear()
{
    snapshots_.clear();
}

bool InputActionMapRuntime::Bind(const std::string& action, const std::string& input)
{
    if (action.empty() || input.empty())
        return false;
    for (const auto& binding : bindings_)
    {
        if (binding.second == input && binding.first != action)
            return false;
    }
    bindings_[action] = input;
    return true;
}

std::string InputActionMapRuntime::Resolve(const std::string& input) const
{
    for (const auto& binding : bindings_)
    {
        if (binding.second == input)
            return binding.first;
    }
    return {};
}

void InputActionMapRuntime::Clear()
{
    bindings_.clear();
}

bool ScreenshotRegressionCatalog::Add(const ScreenshotRegressionCase& test)
{
    if (test.name.empty() || test.expectedDigest == 0 || expected_.find(test.name) != expected_.end())
        return false;
    expected_[test.name] = test.expectedDigest;
    return true;
}

bool ScreenshotRegressionCatalog::Compare(const std::string& name, std::uint64_t actualDigest, std::string* error) const
{
    const auto it = expected_.find(name);
    if (it == expected_.end() || actualDigest == 0 || it->second != actualDigest)
    {
        if (error)
            *error = "screenshot regression mismatch for " + name;
        return false;
    }
    return true;
}

void ScreenshotRegressionCatalog::Clear()
{
    expected_.clear();
}

bool ResourceHotReloadCoordinator::Register(const HotReloadResourceRecord& record)
{
    if (record.resourceId.empty() || record.previousDigest == 0 || record.currentDigest == 0
        || record.previousDigest == record.currentDigest || Find(record.resourceId))
        return false;
    records_.push_back(record);
    return true;
}

bool ResourceHotReloadCoordinator::Apply(const std::string& resourceId, bool stateMigrated)
{
    for (HotReloadResourceRecord& record : records_)
    {
        if (record.resourceId == resourceId)
        {
            if (!stateMigrated)
                return false;
            record.stateMigrated = true;
            return true;
        }
    }
    return false;
}

const HotReloadResourceRecord* ResourceHotReloadCoordinator::Find(const std::string& resourceId) const
{
    for (const HotReloadResourceRecord& record : records_)
    {
        if (record.resourceId == resourceId)
            return &record;
    }
    return nullptr;
}

void ResourceHotReloadCoordinator::Clear()
{
    records_.clear();
}

bool BuildProvenanceLedger::Add(const BuildProvenanceRecord& record)
{
    if (record.step.empty() || record.toolchain.empty() || record.inputDigest == 0 || record.outputDigest == 0)
        return false;
    for (const BuildProvenanceRecord& existing : records_)
    {
        if (existing.step == record.step)
            return false;
    }
    records_.push_back(record);
    return true;
}

bool BuildProvenanceLedger::Validate(std::string* error) const
{
    if (records_.empty())
    {
        if (error)
            *error = "build provenance ledger is empty";
        return false;
    }
    for (const BuildProvenanceRecord& record : records_)
    {
        if (record.step.empty() || record.toolchain.empty() || record.inputDigest == 0 || record.outputDigest == 0)
        {
            if (error)
                *error = "build provenance contains an incomplete record";
            return false;
        }
    }
    return true;
}

std::uint64_t BuildProvenanceLedger::ComputeDigest() const
{
    std::uint64_t digest = 1469598103934665603ull;
    for (const BuildProvenanceRecord& record : records_)
    {
        digest = HashBytes(digest, record.step);
        digest = HashBytes(digest, record.toolchain);
        digest = HashBytes(digest, std::to_string(record.inputDigest));
        digest = HashBytes(digest, std::to_string(record.outputDigest));
    }
    return digest;
}

void BuildProvenanceLedger::Clear()
{
    records_.clear();
}

} // namespace Urho3D
