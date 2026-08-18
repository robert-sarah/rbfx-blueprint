// Copyright (c) 2026 rbfx-blueprint contributors
// SPDX-License-Identifier: MIT

#include <Urho3D/WorldFabric/ProductionFinalization.h>
#include <Urho3D/WorldFabric/ProductionReadiness.h>

#include <catch2/catch_amalgamated.hpp>

using namespace Urho3D;

TEST_CASE("Production native validation matrix never overclaims evidence", "[production][native]")
{
    NativeValidationMatrix matrix;
    REQUIRE(matrix.Add({ProductionPlatform::Linux, true, true, true, false, "CI native"}));
    REQUIRE(matrix.Add({ProductionPlatform::Windows, true, true, true, true, "Windows runner"}));
    REQUIRE(matrix.Add({ProductionPlatform::macOS, true, true, false, false, "Awaiting graphical smoke"}));
    CHECK_FALSE(matrix.Add({ProductionPlatform::Linux, true, true, true, true, "duplicate"}));
    CHECK(matrix.Validate());
    CHECK(matrix.Find(ProductionPlatform::Windows) != nullptr);
    CHECK(matrix.ComputeDigest() != 0);

    NativeValidationMatrix invalid;
    REQUIRE(invalid.Add({ProductionPlatform::macOS, false, true, false, false, "invalid"}));
    CHECK_FALSE(invalid.Validate());
}

TEST_CASE("Production soak runner executes bounded deterministic long runs", "[production][soak]")
{
    LongRunSoakRunner runner;
    LongRunPlan plan;
    plan.frames = 120;
    plan.fixedDeltaSeconds = 1.0 / 60.0;
    plan.checkpointInterval = 30;
    std::uint64_t state = 0;
    const LongRunResult result = runner.Run(plan, [&](std::uint64_t frame, double delta)
    {
        CHECK(delta == plan.fixedDeltaSeconds);
        state += frame + 1;
        return true;
    });
    CHECK(result.success);
    CHECK(result.framesExecuted == 120);
    CHECK(result.checkpoints == 4);
    CHECK(result.simulatedSeconds == Catch::Approx(2.0));
    CHECK(state == 7260);
    CHECK(result.digest != 0);

    plan.frames = 0;
    CHECK_FALSE(runner.Run(plan, [](std::uint64_t, double) { return true; }).success);
}

TEST_CASE("Production performance budgets and frame history are bounded", "[production][performance]")
{
    PerformanceBudgetGate gate;
    PerformanceBudget budget{4.0, 6.0, 16.6, 1024};
    CHECK(gate.Evaluate(budget, {2.0, 3.0, 10.0, 512}));
    CHECK_FALSE(gate.Evaluate(budget, {5.0, 3.0, 10.0, 512}));
    CHECK_FALSE(gate.Evaluate(budget, {2.0, 3.0, 10.0, 2048}));

    FrameMetricsRecorder recorder(3);
    recorder.Record({1.0, 1.0, 3.0, 100});
    recorder.Record({1.0, 1.0, 5.0, 100});
    recorder.Record({1.0, 1.0, 4.0, 100});
    recorder.Record({1.0, 1.0, 10.0, 100});
    CHECK(recorder.GetSamples().size() == 3);
    CHECK(recorder.GetAverageFrameMilliseconds() == Catch::Approx(6.333333));
    CHECK(recorder.GetP95FrameMilliseconds() == Catch::Approx(10.0));
}

TEST_CASE("Animation retargeting and normalized blend graphs are deterministic", "[production][animation]")
{
    AnimationRetargetProfile retarget;
    retarget.sourceRig = "hero-source";
    retarget.targetRig = "hero-runtime";
    retarget.boneMap = {{"root", "root"}, {"hand_l", "hand_l"}, {"hand_r", "hand_r"}};
    ProductionAnimationRetargeter retargeter;
    REQUIRE(retargeter.Validate(retarget));
    CHECK(retargeter.GetMappedBoneCount(retarget) == 3);

    AnimationBlendGraph blend;
    REQUIRE(blend.AddNode({"idle", 1.0f}));
    REQUIRE(blend.AddNode({"aim", 3.0f}));
    const auto normalized = blend.EvaluateNormalized();
    REQUIRE(normalized.size() == 2);
    CHECK(normalized[0].weight == Catch::Approx(0.25f));
    CHECK(normalized[1].weight == Catch::Approx(0.75f));
    CHECK_FALSE(blend.AddNode({"aim", 1.0f}));
}

TEST_CASE("Cinematic timelines interpolate and shot lists reject overlaps", "[production][cinematic]")
{
    CinematicTimeline timeline;
    REQUIRE(timeline.AddKeyframe({0.0, "fov", 60.0f}));
    REQUIRE(timeline.AddKeyframe({2.0, "fov", 40.0f}));
    CHECK(timeline.Evaluate("fov", 1.0) == Catch::Approx(50.0f));
    CHECK(timeline.Evaluate("missing", 1.0, 7.0f) == Catch::Approx(7.0f));

    CinematicShotList shots;
    REQUIRE(shots.AddShot({"intro", 0.0, 2.0, "camera-a"}));
    CHECK_FALSE(shots.AddShot({"overlap", 1.0, 3.0, "camera-b"}));
    REQUIRE(shots.AddShot({"gameplay", 2.0, 6.0, "camera-b"}));
    CHECK(shots.FindActive(3.0)->id == "gameplay");
}

TEST_CASE("Asset import validation accepts modern 3D and 2D source formats", "[production][assets]")
{
    AssetImportValidator validator;
    ProductionAssetImportResult incompatible;
    const bool incompatibleAccepted = validator.Validate({"UI/icon.png", ProductionAssetKind::Model3D, false, false, false}, incompatible);
    CHECK_FALSE(incompatibleAccepted);
}

TEST_CASE("Asset dependencies and export manifests are deterministic and cycle-safe", "[production][assets]")
{
    AssetDependencyManifest dependencies;
    REQUIRE(dependencies.AddAsset("scene", {"hero", "terrain", "hero"}));
    REQUIRE(dependencies.AddAsset("hero", {"hero-texture"}));
    REQUIRE(dependencies.AddAsset("terrain", {"terrain-texture"}));
    REQUIRE(dependencies.AddAsset("hero-texture", {}));
    REQUIRE(dependencies.AddAsset("terrain-texture", {}));
    CHECK(dependencies.ValidateAcyclic());
    CHECK(dependencies.GetTransitiveDependencies("scene").size() == 4);
    CHECK(dependencies.ComputeDigest() != 0);

    AssetDependencyManifest cyclic;
    REQUIRE(cyclic.AddAsset("a", {"b"}));
    REQUIRE(cyclic.AddAsset("b", {"a"}));
    CHECK_FALSE(cyclic.ValidateAcyclic());

    AssetExportPlanner planner;
    AssetExportManifest exportManifest{"windows-release", {"game.exe", "game.pak"}, dependencies.ComputeDigest()};
    REQUIRE(planner.Validate(exportManifest));
    CHECK(planner.ComputeDigest(exportManifest) != 0);
}

TEST_CASE("Visual UI layout resolves anchors and styles", "[production][ui]")
{
    UIStyleSheetRuntime styles;
    REQUIRE(styles.SetStyle({"primary", 8.0f, 4.0f, 0.95f}));
    CHECK(styles.FindStyle("primary")->cornerRadius == Catch::Approx(4.0f));
    CHECK_FALSE(styles.SetStyle({"invalid", 1.0f, 1.0f, 2.0f}));

    UIWidgetLayoutEngine layout;
    const UIWidgetRect rect = layout.Resolve({800.0f, 600.0f, 0.25f, 0.25f, 0.75f, 0.75f, 10.0f, 20.0f, -10.0f, -20.0f});
    CHECK(rect.left == Catch::Approx(210.0f));
    CHECK(rect.top == Catch::Approx(170.0f));
    CHECK(rect.right == Catch::Approx(590.0f));
    CHECK(rect.bottom == Catch::Approx(430.0f));
}

TEST_CASE("Packaging, recovery, input and screenshot gates are production-safe", "[production][packaging]")
{
    PackageManifestBuilder builder;
    ProductionPackageManifest manifest{"rbfx-game", ProductionEngineVersion, ProductionPlatform::Linux, {}, 1234};
    REQUIRE(builder.AddArtifact(manifest, "game"));
    REQUIRE(builder.AddArtifact(manifest, "data.pak"));
    CHECK_FALSE(builder.AddArtifact(manifest, "game"));
    REQUIRE(builder.Validate(manifest));
    CHECK(builder.ComputeDigest(manifest) != 0);

    PackageTargetMatrix targets;
    REQUIRE(targets.Add(ProductionPlatform::Linux));
    REQUIRE(targets.Add(ProductionPlatform::Windows));
    CHECK_FALSE(targets.ValidateDesktopCoverage());
    REQUIRE(targets.Add(ProductionPlatform::macOS));
    CHECK(targets.ValidateDesktopCoverage());

    CrashRecoveryJournal journal;
    REQUIRE(journal.Append({1, "Scenes/Main.scene", "world-state-1"}));
    REQUIRE(journal.Append({2, "Scenes/Main.scene", "world-state-2"}));
    CrashRecoverySnapshot restored;
    REQUIRE(journal.RestoreLatest(restored));
    CHECK(restored.sequence == 2);
    CHECK_FALSE(journal.Append({2, "Scenes/Main.scene", "duplicate"}));

    InputActionMapRuntime inputs;
    REQUIRE(inputs.Bind("jump", "Space"));
    REQUIRE(inputs.Bind("move-left", "A"));
    CHECK(inputs.Resolve("Space") == "jump");
    CHECK_FALSE(inputs.Bind("other", "Space"));

    ScreenshotRegressionCatalog screenshots;
    REQUIRE(screenshots.Add({"main-menu", 9876}));
    CHECK(screenshots.Compare("main-menu", 9876));
    CHECK_FALSE(screenshots.Compare("main-menu", 1234));
}

TEST_CASE("Hot reload and build provenance require explicit production evidence", "[production][hot-reload]")
{
    ResourceHotReloadCoordinator hotReload;
    REQUIRE(hotReload.Register({"player-blueprint", 100, 200, false}));
    CHECK_FALSE(hotReload.Register({"player-blueprint", 200, 300, false}));
    CHECK_FALSE(hotReload.Apply("player-blueprint", false));
    REQUIRE(hotReload.Apply("player-blueprint", true));
    REQUIRE(hotReload.Find("player-blueprint"));
    CHECK(hotReload.Find("player-blueprint")->stateMigrated);

    BuildProvenanceLedger ledger;
    CHECK_FALSE(ledger.Validate());
    REQUIRE(ledger.Add({"compile", "gcc-13", 1001, 2001}));
    REQUIRE(ledger.Add({"package", "cmake-ninja", 2001, 3001}));
    CHECK_FALSE(ledger.Add({"compile", "gcc-13", 1001, 2001}));
    REQUIRE(ledger.Validate());
    CHECK(ledger.ComputeDigest() != 0);
    ledger.Clear();
    CHECK_FALSE(ledger.Validate());
}

TEST_CASE("Production diagnostics are bounded, deterministic and privacy-safe", "[production][diagnostics]")
{
    ProductionDiagnosticLog log(2);
    REQUIRE(log.Add({"RB-001", ProductionDiagnosticSeverity::Warning, "/home/ubuntu/project/main.rbscript", "warning in /Users/alice/project", 4, 8}));
    REQUIRE(log.Add({"RB-002", ProductionDiagnosticSeverity::Error, "Source/Test.cpp", "invalid resource", 9, 2}));
    CHECK_FALSE(log.Add({"RB-003", ProductionDiagnosticSeverity::Info, "Source/Test.cpp", "overflow", 1, 1}));
    CHECK(log.HasErrors());
    CHECK_FALSE(log.HasCritical());
    CHECK(log.Count(ProductionDiagnosticSeverity::Warning) == 1);
    CHECK(log.Serialize().find("<user-path>") != std::string::npos);
    log.Clear();
    CHECK(log.GetRecords().empty());
}

TEST_CASE("Production sanitizer and soak gates reject incomplete evidence", "[production][hardening]")
{
    ProductionSanitizerGate sanitizers;
    SanitizerEvidence evidence{true, true, true, true, true, true, "gcc-13", {}};
    CHECK(sanitizers.Validate(evidence));
    evidence.fuzzing = false;
    CHECK_FALSE(sanitizers.Validate(evidence));

    ProductionSoakGate soak;
    SoakBudget budget{1000, 60.0, 20.0, 2048};
    SoakEvidence result{1200, 75.0, 16.0, 1024, 0, 0, 0};
    CHECK(soak.Validate(budget, result));
    result.crashCount = 1;
    CHECK_FALSE(soak.Validate(budget, result));
}

TEST_CASE("Production native matrix requires real desktop evidence", "[production][native]")
{
    ProductionNativeReleaseGate gate;
    const NativePlatformEvidence linuxEvidence{ProductionPlatform::Linux, "x86_64", "gcc-13", "Vulkan", "sha-linux", true, true, true, true, true, true};
    const NativePlatformEvidence windowsEvidence{ProductionPlatform::Windows, "x86_64", "msvc", "DirectX", "sha-windows", true, true, true, true, true, true};
    const NativePlatformEvidence macosEvidence{ProductionPlatform::macOS, "arm64", "clang", "Metal", "sha-macos", true, true, true, true, true, true};
    REQUIRE(gate.Add(linuxEvidence));
    REQUIRE(gate.Add(windowsEvidence));
    REQUIRE(gate.Add(macosEvidence));
    CHECK_FALSE(gate.Add(linuxEvidence));
    CHECK(gate.ValidateDesktopMatrix());
    CHECK(gate.Find(ProductionPlatform::Windows)->artifactDigest == "sha-windows");
}

TEST_CASE("Production reproducibility, plugins and dependency audit are versioned", "[production][reproducibility]")
{
    const ReproducibleBuildSpec build{"abc123", "gcc", "13.2", "3.28", "1.11", "linux-x86_64", "Release", "deps", "source", "artifact"};
    ReproducibleBuildVerifier verifier;
    CHECK(verifier.Validate(build));
    CHECK(verifier.Equivalent(build, build));
    CHECK(verifier.ComputeDigest(build) != 0);

    ProductionPluginRegistry plugins;
    REQUIRE(plugins.Add({"world-tools", "1.0.0", "1", ">=0.7", "world-tools.so", {"core"}, true}));
    CHECK_FALSE(plugins.Add({"world-tools", "1.0.1", "1", ">=0.7", "world-tools.so", {}, true}));
    CHECK(plugins.Validate());
    CHECK(plugins.Remove("world-tools"));

    ProductionDependencyAudit dependencies;
    REQUIRE(dependencies.Add({"fmt", "10.2", "MIT", "sha-fmt", 0}));
    REQUIRE(dependencies.Add({"catch2", "3.5", "BSL-1.0", "sha-catch2", 0}));
    CHECK(dependencies.Validate());
    CHECK(dependencies.ComputeDigest() != 0);
}

TEST_CASE("Production reference projects and release readiness expose missing gates", "[production][release]")
{
    ReferenceProjectCatalog projects;
    const ReferenceProjectEvidence complete{"reference-2d", "Examples/2D", "sha-2d", "linux", true, true, true, true, true, true};
    REQUIRE(projects.Add(complete));
    REQUIRE(projects.Add({"reference-3d", "Examples/3D", "sha-3d", "linux", true, true, true, true, true, true}));
    REQUIRE(projects.Add({"reference-hybrid", "Examples/Hybrid", "sha-hybrid", "linux", true, true, true, true, true, true}));
    CHECK(projects.ValidateDesktopCoverage());

    ProductionDocumentationEvidence docs{true, true, true, true, true, true, true, true, true, true};
    ReleasePackagingEvidence packaging{"1.0.0", "linux-x86_64", "abc123", "sha-archive", true, true, true, true, true};
    ProductionReleaseGate release;
    CHECK(release.Validate(packaging, docs));

    ProductionReadinessInput input;
    input.sanitizers = {true, true, true, true, true, true, "gcc-13", {}};
    input.soakBudget = {1000, 60.0, 20.0, 2048};
    input.soakEvidence = {1200, 75.0, 16.0, 1024, 0, 0, 0};
    input.documentation = docs;
    input.packaging = packaging;
    input.reproducibleBuild = {"abc123", "gcc", "13.2", "3.28", "1.11", "linux-x86_64", "Release", "deps", "source", "artifact"};
    input.nativeDesktopMatrix = true;
    input.referenceProjects = true;
    input.dependencyAudit = true;
    input.performanceBudgets = true;
    ProductionReadinessEvaluator evaluator;
    const ProductionReadinessResult ready = evaluator.Evaluate(input);
    CHECK(ready.ready);
    CHECK(ready.blocked.empty());

    input.nativeDesktopMatrix = false;
    const ProductionReadinessResult blocked = evaluator.Evaluate(input);
    CHECK_FALSE(blocked.ready);
    CHECK_FALSE(blocked.blocked.empty());
}
