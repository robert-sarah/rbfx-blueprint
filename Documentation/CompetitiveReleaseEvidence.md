# Competitive Release Evidence

## Recorded validation

The following evidence was recorded from the `blueprint-foundation` branch after the rollback, temporal replay, RbScript workspace, 1v1 demonstration, Scene workspace, AI/network/UI contracts, and phase 6 editor and asset-pipeline changes.

| Check | Result | Interpretation |
| --- | --- | --- |
| Linux CMake configuration | Passed | Testing configuration completed with editor and player disabled |
| `Tests` target compilation | Passed | Catch2 test binary built successfully |
| Last clean full CTest run before phase 6 | **393/393 passed** | No failed test cases in the recorded Linux run; the phase 6 source tree now contains 401 cases and needs a fresh coherent rebuild for a new full-suite claim |
| Rollback diagnostics | Covered | Prediction, authoritative digest comparison, divergence reporting, and resynchronization are exercised |
| 1v1 competitive demo | Covered | Delayed remote input creates measurable divergence and rollback restores exact convergence |
| Time-machine replay persistence | Covered | JSON export/import is round-tripped and tampered digests are rejected atomically |
| RbScript workspace tooling | Covered | Cross-file symbols, definitions, references, and JSON-RPC capabilities are tested |
| Scene 2D editor units | Passed | `SceneViewTab.cpp` and `TransformManipulator.cpp` compile through targeted EditorLibrary Ninja targets |
| Phase 5 production units | Passed | Behavior tree, online session, and accessibility units compile and their targeted tests pass; the full suite reached 393/393 |
| Asset cache manifest | Passed | `AssetPipeline` manifest persistence and validation units compile with deterministic ordering and duplicate rejection |
| Terrain authoring core | Passed | Deterministic falloffs, quantized stamping, digest stability, and invalid settings are covered by targeted compilation and smoke validation |
| World Fabric terrain panel | Passed | `WorldFabricTab.cpp` compiles with the new authoring state and ImGui controls using the editor include configuration |
| Shader Graph hardening | Passed at compile level | Validation and HLSL sampler tests compile; a full runtime test remains tied to a coherent fresh Tests build because mixing old and new shared-library ABI artifacts is invalid |
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

The final clean CTest report before phase 6 was:

```text
100% tests passed, 0 tests failed out of 393
```

The phase 6 source tree contains 401 `TEST_CASE` declarations. The complete post-phase-6 CTest report is deliberately not claimed yet: the fresh validation directory was stopped while rebuilding the complete third-party graph, so the remaining step is to reuse one coherent build directory and run the full `Tests` target followed by CTest.

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
| `680a9aa` | Shader Graph validation and HLSL sampler declarations |

All listed commits are published on [`blueprint-foundation`](https://github.com/robert-sarah/rbfx-blueprint/tree/blueprint-foundation).

## Evidence boundaries

This evidence is strong for the deterministic contracts and testable production services covered by the repository. The targeted phase 6 results are separated from the last complete CTest run so that the documentation does not overstate validation. It is not a substitute for native graphical certification on every supported operating system, a long-duration network soak under the target packet-loss profile, or validation of a specific commercial game's third-party physics, audio, rendering, or platform middleware. Those checks remain release gates for each project that adopts the engine.

## References

1. [`CompetitiveMission.md`](CompetitiveMission.md)
2. [`CompetitiveNetcode.md`](CompetitiveNetcode.md)
3. [`DeterministicTimeMachine.md`](DeterministicTimeMachine.md)
4. [`ProductionReadiness.md`](ProductionReadiness.md)
