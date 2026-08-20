# Competitive Release Evidence

## Recorded validation

The following evidence was recorded from the `blueprint-foundation` branch after the rollback, temporal replay, RbScript workspace, 1v1 demonstration, Scene workspace, AI/network/UI contracts, phase 6 editor and asset-pipeline changes, and the deterministic model-import profile.

| Check | Result | Interpretation |
| --- | --- | --- |
| Linux CMake configuration | Passed | Testing configuration completed with editor and player disabled |
| `Tests` target compilation | Passed | Catch2 test binary built successfully |
| Coherent full CTest run after model-import validation | **411/411 passed** | No failed test cases in the reconfigured Linux build; BuildGraph cooking-plan, editor production validation, model-import profile and AssetPipeline integration tests are included |
| Rollback diagnostics | Covered | Prediction, authoritative digest comparison, divergence reporting, and resynchronization are exercised |
| 1v1 competitive demo | Covered | Delayed remote input creates measurable divergence and rollback restores exact convergence |
| Time-machine replay persistence | Covered | JSON export/import is round-tripped and tampered digests are rejected atomically |
| RbScript workspace tooling | Covered | Cross-file symbols, definitions, references, and JSON-RPC capabilities are tested |
| Scene 2D editor units | Passed | `SceneViewTab.cpp` and `TransformManipulator.cpp` compile through targeted EditorLibrary Ninja targets |
| Phase 5 production units | Passed | Behavior tree, online session, and accessibility units compile and their targeted tests pass |
| Asset cache manifest | Passed | `AssetPipeline` manifest persistence and validation units compile with deterministic ordering and duplicate rejection |
| Terrain authoring core | Passed | Deterministic falloffs, quantized stamping, digest stability, and invalid settings are covered by targeted compilation and smoke validation |
| World Fabric terrain panel | Passed | `WorldFabricTab.cpp` compiles with the new authoring state and ImGui controls using the editor include configuration |
| Shader Graph hardening | Passed | Validation and HLSL sampler tests pass in the coherent full Tests build; mixing old and new shared-library ABI artifacts remains invalid |
| Editor production validation | Passed | 2D, 3D, Blueprint, RbScript and Shader Graph reference scenarios validate; unsafe scene, autosave, recovery and asset states produce stable diagnostics |
| Model import profile | Passed | Units, axes, handedness, tangents, UV policy, descending LOD thresholds, provenance, JSON completeness and digest normalization are covered |
| Windows runtime package | Available | The fixed package uses a statically linked C++ runtime and includes the required runtime resources |

The test command used for the clean Linux evidence was:

```bash
cmake -S . -B build-tests -G Ninja \
  -DURHO3D_TESTING=ON \
  -DURHO3D_EDITOR=OFF \
  -DURHO3D_PLAYER=OFF \
  -DURHO3D_CSHARP=OFF
cmake --build build-tests --target Tests -j2
ctest --test-dir build-tests --output-on-failure
```

The final coherent model-import validation CTest report is:

```text
100% tests passed, 0 tests failed out of 411
Total Test time (real) = 250.60 sec
```

The phase 9 cases cover case-insensitive model extensions, empty source data, missing cooked output paths, mismatched importer profiles, and normalized rule removal. The phase 10 cases cover required metadata for LOD, texture and provenance tasks, plus digest stability under metadata insertion-order changes. The phase 11 cases cover reference editor workspaces, 2D/3D exclusivity, autosave bounds and readiness, recovery warnings, and stable validation digests. The model-import cases cover complete profile JSON round trips, format normalization, unsafe units/UV/LOD values, schema completeness, provenance-sensitive digests, effective AssetImportSettings JSON persistence, cache-hash participation, early importer rejection, cache reuse, and profile-driven invalidation. They are included in the 411 registered Catch2 cases.

## Published commits

| Commit | Scope |
| --- | --- |
| `eebc4dd` | Rollback diagnostics, resynchronization, and deterministic state digest |
| `0207c4c` | Bounded prediction and desync validation |
| `776ca7c` | Executable 1v1 rollback demonstration |
| `36c373f` | Deterministic time-machine JSON replay persistence |
| `c1246f8` | Workspace-aware RbScript language tooling |
| `07e1806` | Scene 2D/3D profile parity and world-aligned XY authoring |
| `68ff70c` | Typed RbScript highlighting and production render planning |
| `ab27735` | Production physics, audio, and animation contracts |
| `d6ea67b` | AI behavior tree, online session, and accessibility contracts |
| `7a35573` | Reproducible asset-cache manifests |
| `77da649` | Deterministic terrain authoring and World Fabric panel |
| `680a9aa` | Shader Graph hardening and HLSL sampler declarations |
| `871762f` | Correct terrain falloff test assertion and preserve deterministic decay guarantees |
| `a7c3718` | Record coherent 399-test validation and clarify source-test counting |
| `ad138b5` | Harden deterministic asset import validation and publish the 401/401 evidence |
| `3730cd2` | Add deterministic BuildGraph LOD, texture and provenance task schemas |
| `deb2923` | Add editor production validation scenarios and CMake test integration |
| `756f09d` | Add deterministic model import profiles and documentation |

All commits through `deb2923` are published on [`blueprint-foundation`](https://github.com/robert-sarah/rbfx-blueprint/tree/blueprint-foundation). The model-profile commits `756f09d` and `a67df22` are committed locally with passing validation and are pending publication because the current GitHub CLI credential has expired; no local changes are lost.

## Evidence boundaries

This evidence is strong for the deterministic contracts and testable production services covered by the repository. The 411/411 result comes from one coherent Linux build directory after the model-import changes; it is not assembled by mixing objects or shared libraries from different configurations. It is not a substitute for native graphical certification on every supported operating system, a long-duration network soak under the target packet-loss profile, or validation of a specific commercial game's third-party physics, audio, rendering, or platform middleware. Those checks remain release gates for each project that adopts the engine.

## References

1. [`CompetitiveMission.md`](CompetitiveMission.md)
2. [`CompetitiveNetcode.md`](CompetitiveNetcode.md)
3. [`DeterministicTimeMachine.md`](DeterministicTimeMachine.md)
4. [`ProductionReadiness.md`](ProductionReadiness.md)
