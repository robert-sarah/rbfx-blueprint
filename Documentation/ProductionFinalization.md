# Production Finalization 0.7.0

## Scope

`rbfx-blueprint` version **0.7.0-production** consolidates the production-oriented runtime contracts that sit above the rbfx core. The contracts are deterministic, bounded, testable without a graphics device, and designed to be connected to real platform backends by the editor, game runtime, or build farm.

> A passing contract test proves the behavior of the service in the tested configuration. It does not by itself certify a GPU driver, a native window manager, a platform SDK, or a shipping game.

## Twenty production extensions

| # | Extension | Purpose |
| --- | --- | --- |
| 1 | `NativeValidationMatrix` | Records configured, compiled, tested and graphical-smoke evidence per platform without overclaiming unexecuted stages. |
| 2 | `LongRunSoakRunner` | Executes bounded deterministic endurance plans with checkpoints, simulated time and a digest. |
| 3 | `PerformanceBudgetGate` | Rejects CPU, GPU, frame-time and memory samples that exceed explicit budgets. |
| 4 | `FrameMetricsRecorder` | Keeps a bounded frame history and computes deterministic average and P95 frame time. |
| 5 | `AnimationRetargeter` | Validates source/target rigs and explicit bone mappings for reusable character animation. |
| 6 | `AnimationBlendGraph` | Normalizes deterministic animation blend weights and rejects malformed nodes. |
| 7 | `CinematicTimeline` | Stores ordered keyframes and interpolates cinematic properties. |
| 8 | `CinematicShotList` | Validates non-overlapping camera shots and resolves the active shot at a time. |
| 9 | `AssetImportValidator` | Validates 2D, 3D, hybrid, glTF/GLB, OBJ and texture import intents against project capabilities. |
| 10 | `AssetDependencyManifest` | Builds a cycle-safe transitive dependency manifest with a stable digest. |
| 11 | `AssetExportPlanner` | Validates platform export targets and computes a reproducible artifact digest. |
| 12 | `UIStyleSheetRuntime` | Stores validated runtime UI styles with bounded opacity, padding and corner radius. |
| 13 | `UIWidgetLayoutEngine` | Resolves anchored widget rectangles and normalizes inverted bounds. |
| 14 | `PackageManifestBuilder` | Builds and validates a deterministic `ProductionPackageManifest` without colliding with rbfx’s native `PackageManifest`. |
| 15 | `PackageTargetMatrix` | Requires Linux, Windows and macOS coverage for desktop packaging. |
| 16 | `CrashRecoveryJournal` | Keeps ordered recovery snapshots and restores the latest valid world state. |
| 17 | `InputActionMapRuntime` | Enforces one-to-one runtime action bindings and deterministic input resolution. |
| 18 | `ScreenshotRegressionCatalog` | Compares named visual digests for regression gates. |
| 19 | `ResourceHotReloadCoordinator` | Requires explicit state-migration evidence when a resource changes during runtime. |
| 20 | `BuildProvenanceLedger` | Records toolchain, input digest and output digest for reproducible build evidence. |

## Native validation workflow

The repository contains `.github/workflows/production-validation.yml`. It defines three complementary validation layers:

1. `native-contracts` configures, builds and runs the complete CTest suite on Ubuntu, Windows and macOS runners.
2. `editor-build` configures and compiles the native Editor target on all three desktop runners.
3. `linux-graphical-smoke` launches the editor for a bounded period under Xvfb and stores the launch log as an artifact.

An optional `workflow_dispatch` input named `native_gui_smoke` attempts a native Windows and macOS editor launch. Hosted runners can compile and test those platforms in CI, but a graphical acceptance result is only meaningful when the runner has a usable display session and the artifact log confirms the application stayed alive. For release certification, execute this job on a project-owned Windows and macOS machine with the target GPU drivers and record the `NativeValidationMatrix` evidence.

## Local validation

The complete Linux test suite can be run with:

```bash
cmake -S . -B build -G Ninja \
  -DURHO3D_TESTING=ON \
  -DURHO3D_EDITOR=OFF \
  -DURHO3D_PLAYER=OFF \
  -DURHO3D_CSHARP=OFF \
  -DURHO3D_PROFILING_FALLBACK=ON
cmake --build build --target Tests -j2
ctest --test-dir build --output-on-failure
```

The production subset is useful for fast iteration:

```bash
./build/bin/Debug/Tests '[production]' --reporter compact
```

For an editor build on Linux:

```bash
cmake -S . -B build-editor -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DURHO3D_EDITOR=ON \
  -DURHO3D_PLAYER=OFF \
  -DURHO3D_TESTING=OFF \
  -DURHO3D_CSHARP=OFF
cmake --build build-editor --target Editor -j2
```

## Packaging evidence

A release package should include the engine commit, `ProductionEngineVersion`, platform, compiler/toolchain, World Fabric digest, asset manifest digest, package artifact digest and the native test result. `BuildProvenanceLedger` is intended to store the toolchain and digest chain; `PackageTargetMatrix` rejects a desktop release manifest that omits one of Linux, Windows or macOS.

The final release process should still perform real asset cooking, signing, installer generation, GPU-driver testing, crash-report verification and launch testing on each supported operating system. Those operations depend on the target project, credentials and hardware and cannot be honestly marked complete from a Linux-only sandbox.

## ProductionReadiness 1.0 gates

The complementary `ProductionReadiness` contracts in `Source/Urho3D/WorldFabric/ProductionReadiness.*` turn the remaining production requirements into explicit evidence gates. They cover privacy-safe diagnostics, sanitizer and fuzzing evidence, long-run soak budgets, native desktop release matrices, reproducible builds, plugin ABI manifests, dependency and license audits, 2D/3D/hybrid reference projects, documentation coverage and release packaging.

The full gate matrix and the recommended 1.0 exit criteria are documented in [`Documentation/ProductionReadiness.md`](ProductionReadiness.md). The current Linux test validation contains **370/370 CTest tests** after integrating these contracts. This count verifies the tested Linux contracts; it does not replace native Windows/macOS graphical execution, GPU-driver validation, signing, installer testing or acceptance by an external project team.

The workflow now includes a reproducible-release evidence job and a Linux ASan/UBSan production-contract job. These jobs publish the commit, toolchain, system and executable hashes as artifacts. Native platform certification still requires the corresponding real runners and target hardware.
