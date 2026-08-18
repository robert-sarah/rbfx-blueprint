# rbfx-blueprint

**rbfx-blueprint** is a **C++17 2D and 3D game engine and framework** based on the [rbfx](https://github.com/rbfx/rbfx) fork of [Urho3D](https://github.com/urho3d/Urho3D). It preserves the control of a code-first engine while adding an integrated production toolchain: an extensible editor, Blueprint visual graphs, the typed rbscript gameplay language, rendering and content tools, correlated diagnostics, and semantic orchestration through **World Fabric**.

[![Native validation](https://github.com/robert-sarah/rbfx-blueprint/actions/workflows/build.yml/badge.svg?branch=blueprint-foundation)](https://github.com/robert-sarah/rbfx-blueprint/actions/workflows/build.yml)

> **Project status:** active development branch. The P0 foundations, most P1 features, the main P2 panels, and the P3/P4 foundations are present in the repository. The project is not presented as a final release certified for every platform.

## Vision

rbfx-blueprint brings three production layers into one engine:

| Layer | Role | Integration |
| --- | --- | --- |
| **C++** | Runtime, rendering, physics, networking, tools, and native extensions | Shares the rbfx reflection API with the other layers |
| **Blueprint** | Visual graphs for gameplay, logic, tools, and production | Reflected nodes, subgraphs, comments, search, and editor integration |
| **rbscript** | Typed gameplay language with brace-based syntax | Designed to use the same rbfx reflection system as C++ and Blueprint |
| **World Fabric** | Cross-system semantic graph | Connects resources, dependencies, builds, simulation, profiling, networking, and production |

## World Fabric: the project differentiator

**World Fabric** is the engine's semantic dependency graph. It does not represent files only: each node can describe a scene, asset, shader, Blueprint graph, script, build task, simulation, or runtime system. Dependencies are persisted in JSON resources and can be analyzed, ordered, profiled, and queried from the editor.

This architecture connects cause and effect across the production pipeline. A resource change can be traced to its consumers, invalidated tasks, temporal events, and CPU/GPU costs. Associated runtime services include transitive impact analysis, semantic queries, correlated profiling, deterministic simulation, and versioned collaboration operations.

## Available features

### P0 — Editor foundations

The P0 layer establishes a consistent editor experience: a shared visual system, docking, workspaces, a command palette, autosave and recovery, Asset Browser, Inspector and Outliner filters, common UI contracts, and associated automated tests.

### P1 — Integrated production pipeline

The following editors and resources are available on the `blueprint-foundation` branch:

| Domain | Features |
| --- | --- |
| **Rendering** | **Shader Graph** resource and editor |
| **Effects** | **VFX Graph** resource and editor |
| **Audio** | **Audio Mixer** resource and editor |
| **Animation and cinematics** | **Sequencer** resource and editor |
| **Build** | **Build Dashboard** and deterministic build resource |
| **World** | Production inspectors for Terrain, TileMap2D, and NavigationMesh |
| **Multiplayer** | **Multiplayer Profile**, startup settings, replication, and diagnostics |
| **Resource routing** | Dedicated extensions registered through `StandardFileTypes` |

These components provide editor contracts, JSON persistence, validation, and integration points for a broader production toolchain. They do not yet replace a complete certification matrix for Windows, macOS, Linux, mobile, WebAssembly, and consoles.

### P2 — World Fabric and correlated production

The P2 layer includes:

| Service | Capabilities |
| --- | --- |
| **Dependency Explorer** | Node and edge editing, inspection, and build ordering |
| **Impact Analysis** | Transitive impact propagation through the graph |
| **Semantic Query** | Search by node type, tags, and metadata |
| **Correlated Profiler** | Correlation between graph nodes and runtime measurements |
| **Semantic Timeline** | Association between World Fabric nodes and Sequencer timeline events |
| **Incremental Scheduler** | Visualization of invalidation and task state by node |
| **Deterministic Reproduction** | Snapshot history, restore, replay, and reproduction workflows |
| **Collaboration** | Known clients, locking, operations, revisions, and synchronization diagnostics |

The P2 panels are integrated into `WorldFabricTab` and use runtime services from `Source/Urho3D/WorldFabric/` rather than isolated interface-only data.

### P3 — Ecosystem and extensible foundations

The P3 layer extends World Fabric beyond the editor and provides contracts for distributed production:

| Service | Delivered capabilities |
| --- | --- |
| **PluginRegistry and PluginSDK** | Versioned manifests, capabilities, dependencies, cycle detection, deterministic activation order, ABI versioning, reflection/Blueprint/rbscript/editor callbacks, and reproducible digests |
| **DistributedPackageRegistry** | Package registry, semantic version resolution, replication plans, controlled replacements, JSON persistence, and stable digests |
| **WorldFabricRealtimeSession** | Multi-user presence, Lamport clock, ordered envelopes, acknowledgements, and active-client tracking |
| **RbScriptLspService** | LSP/JSON-RPC protocol, document open/update, diagnostics, completion, hover, go-to-definition, rename, and rbfx reflection integration |
| **InteractiveDocumentation** | Page and symbol index, search, Markdown/HTML rendering, type-registry import, and JSON serialization |
| **IncrementalScheduler** | Transitive invalidation, dependency propagation, cycle detection, ready tasks, and deterministic digests |
| **ContentAddressedCache** | Content-addressed artifact cache, SHA-256/FNV-1a digests, revisions, and versioned replacement |
| **GameplayTestHarness** | Deterministic callback execution, stable ordering, seeds, frame limits, results, digests, and shared-library ABI-safe test utilities |
| **HotReloadStateStore** | Runtime field capture/restoration, hot-reload generations, validation, removal, and stable digests |
| **Native CI** | `blueprint-native-validation` job covering Linux, Windows MSVC x64, and macOS arm64/x64 in GitHub Actions |

Local validation of this delivery was performed on a clean Linux rebuild: **336/336 CTest tests pass**. Coverage includes negative cases for deterministic branches, malformed JSON, duplicates, causal detachment, and canonicalization of fields containing separators. The CI matrix prepares native Windows and macOS validation; it does not replace a graphical smoke test executed on each operating system.

### Unique production extensions

The following three services extend World Fabric beyond a dependency graph and are integrated into `WorldFabricTab`:

| Extension | Professional capability |
| --- | --- |
| **Causal World Fabric Debugger** | Captures sequenced causal evidence, analyzes dependency chains, summarizes causes, computes impacted nodes, and supports manual diagnosis from a selected node. |
| **Universal Deterministic Time Machine** | Provides bounded multi-domain history, per-frame states and inputs, restoration, replay, investigation branches, frame comparison, and first-divergence search between branches. |
| **Semantic Build Capsule** | Stores a canonical JSON capsule containing environment data, World Fabric/Time Machine digests, semantic inputs, and plugins, with validation, deterministic fingerprints, and build-to-build diffing. |

These services are designed as reusable runtime contracts for the editor, CI, profiler, networking, and support tools. They provide a foundation for traceability and reproduction; they do not by themselves constitute complete production certification or automatic capture of every engine subsystem.

## Repository architecture

```text
Source/
├── Urho3D/
│   ├── Blueprint/       Visual graph runtime and reflection
│   ├── RbScript/        rbscript language and compilation
│   ├── WorldFabric/     Semantic graph, simulation, profiling, and plugins
│   ├── Graphics/        Rendering, RenderGraph, and graphics resources
│   ├── Network/         Networking runtime and multiplayer profiles
│   └── ...              C++ rbfx/Urho3D subsystems
├── Editor/
│   ├── Foundation/      Production editor tabs and contracts
│   └── ...              Editor applications and extensions
└── Tests/               Catch2 v3 tests for resources and services
```

C++ files in the main source directories are discovered automatically by the existing CMake configuration. Production resources follow rbfx conventions for loading, JSON persistence, reflection, and validation.

## Requirements

Primary development uses **C++17**, **CMake**, **Ninja**, and a compiler compatible with GCC 13 or an equivalent toolchain. On Linux, the usual OpenGL, Vulkan, X11, DBus, and system graphics dependencies must be available. Third-party engine dependencies are managed by the repository and its CMake configuration.

## Build and test on Linux

From the repository root:

```bash
cmake -S . -B build -G Ninja \
  -DURHO3D_TESTING=ON \
  -DURHO3D_EDITOR=OFF \
  -DURHO3D_PLAYER=OFF \
  -DURHO3D_CSHARP=OFF

cmake --build build --target Tests -j2
ctest --test-dir build --output-on-failure
```

To build the Linux editor in Debug mode:

```bash
cmake -S . -B build-editor -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DURHO3D_EDITOR=ON \
  -DURHO3D_PLAYER=OFF \
  -DURHO3D_TOOLS=OFF \
  -DURHO3D_TESTING=OFF \
  -DURHO3D_CSHARP=OFF

cmake --build build-editor --target Editor -j2
```

The validated Linux editor build in this branch produces `build-editor/bin/Debug/Editor`. The test build produces the Catch2 binary under `build/bin/`, depending on the CMake configuration.

## Portability

The rbfx base targets desktop environments and uses a portable CMake architecture. Complete validation must nevertheless be distinguished from theoretical build capability:

| Platform | Documented status on this branch |
| --- | --- |
| **Linux x86_64** | Configuration, test suite, and editor compilation validated in the development environment |
| **Windows** | Configuration is present; native execution and graphical smoke testing remain to be performed |
| **macOS** | Configuration is present; native macOS smoke testing remains to be completed |
| **Android, iOS, WebAssembly, consoles** | Adapters and validation matrices remain subject to the available toolchains |

Contributions that add a platform should provide a configuration command, a reproducible build, and a native smoke test when the user interface is involved.

## Contributing

Contributions must follow the C++17 conventions and rbfx/EASTL containers used by the project. New resources should define stable persistence, negative validation, a digest where appropriate, and a Catch2 test. Editor extensions should follow `ResourceEditorTab` contracts, use the existing undo mechanisms, and avoid passing const pointers to mutable ImGui fields.

Before opening a pull request, run at least `git diff --check`, the `Tests` target, and the Editor build when modifying `Source/Editor/`. Build artifacts such as `build/` and `build-editor/` must not be committed.

## License and provenance

rbfx-blueprint is distributed under the MIT License. The project is derived from rbfx and Urho3D; upstream copyright notices are preserved in [LICENSE](LICENSE). Contributions specific to rbfx-blueprint are attributed to the authors and contributors of this fork.

## Links

| Resource | Link |
| --- | --- |
| rbfx-blueprint repository | [github.com/robert-sarah/rbfx-blueprint](https://github.com/robert-sarah/rbfx-blueprint) |
| Current development branch | [`blueprint-foundation`](https://github.com/robert-sarah/rbfx-blueprint/tree/blueprint-foundation) |
| Upstream rbfx project | [github.com/rbfx/rbfx](https://github.com/rbfx/rbfx) |
| Upstream Urho3D project | [github.com/urho3d/Urho3D](https://github.com/urho3d/Urho3D) |
| License | [LICENSE](LICENSE) |

## Maturity status

rbfx-blueprint now has a substantially broader editor and production foundation than a minimal prototype: resources are persisted, interface contracts are tested, World Fabric provides a cross-system semantic layer, and the P3/P4 ecosystem is present. **336/336 Linux tests pass** in the current validation session. Full industrial-production qualification still requires load testing, reference projects, native Windows/macOS validation, broader user documentation, and long-duration stability campaigns.

## References

1. [rbfx repository](https://github.com/rbfx/rbfx)
2. [Urho3D repository](https://github.com/urho3d/Urho3D)
3. [GitHub Actions documentation](https://docs.github.com/en/actions)
