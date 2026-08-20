# Competitive Release Evidence

## Recorded validation

The following evidence was recorded from the `blueprint-foundation` branch after the rollback, temporal replay, RbScript workspace, 1v1 demonstration, and Scene workspace changes.

| Check | Result | Interpretation |
| --- | --- | --- |
| Linux CMake configuration | Passed | Testing configuration completed with editor and player disabled |
| `Tests` target compilation | Passed | Catch2 test binary built successfully |
| Full CTest run | **379/379 passed** | No failed test cases in the recorded Linux run |
| Rollback diagnostics | Covered | Prediction, authoritative digest comparison, divergence reporting, and resynchronization are exercised |
| 1v1 competitive demo | Covered | Delayed remote input creates measurable divergence and rollback restores exact convergence |
| Time-machine replay persistence | Covered | JSON export/import is round-tripped and tampered digests are rejected atomically |
| RbScript workspace tooling | Covered | Cross-file symbols, definitions, references, and JSON-RPC capabilities are tested |
| Scene 2D editor units | Passed | `SceneViewTab.cpp` and `TransformManipulator.cpp` compile through targeted EditorLibrary Ninja targets |
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

The final CTest report was:

```text
100% tests passed, 0 tests failed out of 379
Total Test time (real) = 234.34 sec
```

## Published commits

| Commit | Scope |
| --- | --- |
| `eebc4dd` | Rollback diagnostics, resynchronization, and deterministic state digest |
| `0207c4c` | Bounded prediction and desync validation |
| `776ca7c` | Executable 1v1 rollback demonstration |
| `36c373f` | Deterministic time-machine JSON replay persistence |
| `c1246f8` | Workspace-aware RbScript language tooling |
| `07e1806` | Scene 2D/3D profile parity and world-aligned XY authoring |

All listed commits are published on [`blueprint-foundation`](https://github.com/robert-sarah/rbfx-blueprint/tree/blueprint-foundation).

## Evidence boundaries

This evidence is strong for the deterministic contracts and testable production services covered by the repository. It is not a substitute for native graphical certification on every supported operating system, a long-duration network soak under the target packet-loss profile, or validation of a specific commercial game's third-party physics, audio, rendering, or platform middleware. Those checks remain release gates for each project that adopts the engine.

## References

1. [`CompetitiveMission.md`](CompetitiveMission.md)
2. [`CompetitiveNetcode.md`](CompetitiveNetcode.md)
3. [`DeterministicTimeMachine.md`](DeterministicTimeMachine.md)
4. [`ProductionReadiness.md`](ProductionReadiness.md)
