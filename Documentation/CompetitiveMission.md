# Competitive Multiplayer Mission

## Positioning

`rbfx-blueprint` is positioned as a **reference engine foundation for competitive indie and AA multiplayer games**. The project deliberately optimizes for deterministic gameplay, rollback, reproducible debugging, and a small-team production workflow instead of trying to match Unity, Unreal Engine, or Godot in breadth.

The release target is not a marketing claim that every platform, renderer, middleware package, or console SDK is certified. It is a focused engine contract: a team should be able to build a deterministic competitive prototype, observe divergence, reproduce a match frame by frame, and move between C++, Blueprint, and RbScript without leaving the engine's reflection and World Fabric model.

## Primary pillar: deterministic netcode and rollback

The competitive runtime is built from four concrete contracts:

| Contract | Production role | Evidence in this branch |
| --- | --- | --- |
| `DeterministicSimulation` | Fixed-step inputs, bounded state history, restore, replay, and deterministic state digest | Runtime implementation and production tests |
| `RollbackManager` | Ordered local prediction, bounded input history, authoritative reconciliation, digest diagnostics, and transport-independent resynchronization | Runtime implementation and network production tests |
| `UniversalDeterministicTimeMachine` | Branches, restore, replay, frame comparison, divergence search, and JSON replay persistence | Runtime implementation, round-trip tests, and editor integration |
| `TestCompetitiveDemo` | Two-player 1v1 combat loop with delayed remote input, measurable divergence, rollback, and exact convergence | Executable Catch2 demonstration |

`RollbackManager` intentionally does not own a socket or replication transport. The network layer delivers authoritative inputs or checkpoints; the manager validates frame order and digest evidence, restores the required checkpoint, replays the bounded input history, and reports whether the predicted state converged. This separation keeps the deterministic contract testable in isolation and avoids hiding transport-specific assumptions inside simulation code.

## Secondary differentiators

The **Universal Deterministic Time Machine** turns a rollback failure into an inspectable artifact. A development build can preserve a bounded history, create investigation branches, compare frame state, search for the first divergence, and export/import a validated JSON replay. The JSON format is intentionally canonical and rejects malformed ordering or tampered digests atomically.

**RbScript** is a typed gameplay language integrated with the rbfx reflection model. Its LSP service now supports document lifecycle operations, completion, definitions, hover, rename, diagnostics, workspace symbols, and cross-document references. `Examples/RbScript/CompetitivePlayer.rbscript` demonstrates fixed-step input capture and explicit frame state for competitive gameplay. The Blueprint bridge remains bidirectional and is covered by dedicated interoperability tests.

The **2D and 3D profiles share the same scene, selection, undo, resource, World Fabric, Blueprint, RbScript, and testing contracts**. The Scene workspace now provides a persistent 2D profile with an orthographic XY camera, world-aligned grid, configurable grid and snap spacing, XY transform constraints, Z-axis rotation, and a separate four-pane 3D profile. The 2D profile is an authoring contract, not a claim that sprite import, tilemap authoring, or 2D skeletal animation are already complete.

## Recommended competitive workflow

A competitive project should keep simulation state in explicit serializable components and advance it at a fixed tick. Local input is inserted into the rollback history immediately. Remote input is applied when received; if it arrives late, the project restores the latest valid checkpoint, replays the ordered input history, and records the authoritative, predicted, and corrected digests. A non-matching digest should be retained as evidence and should trigger a resynchronization policy chosen by the game, such as requesting a checkpoint or terminating the match when the divergence exceeds the configured window.

World Fabric can associate the match, replay, build capsule, assets, and diagnostics as semantic dependencies. This makes a desync report more useful than a log line: a team can attach the first divergent frame, the branch digest, the build provenance, the relevant gameplay resource, and the exact replay payload to one reproducible investigation record.

## Release evidence and remaining limits

| Area | Current status | Honest limitation |
| --- | --- | --- |
| Deterministic simulation | Implemented and tested | Game-specific physics and third-party middleware still require explicit deterministic adapters |
| Rollback and prediction | Implemented, bounded, digest-aware, and demonstrated by a 1v1 test | No built-in socket transport or matchmaking service is claimed |
| Temporal debugging | Implemented with branch/replay/diff and JSON persistence | Long-duration replay storage and production telemetry need project-specific budgets |
| RbScript tooling | Workspace-aware LSP and examples are present | A full compiler/runtime debugger with every IDE feature remains future work |
| Blueprint interoperability | Bidirectional bridge is tested | Large-project graph conventions and team governance remain product decisions |
| 2D/3D authoring | Shared profile plus dedicated 2D viewport workflow | Specialized 2D content editors are not implied by the viewport mode |
| Desktop validation | Linux configuration and targeted editor compilation are available; native Windows packaging was fixed | Native Windows/macOS graphical certification still requires real runners and display/GPU evidence |

The project should use the following release gates before calling a competitive game production-ready: deterministic replay agreement across supported machines, bounded rollback under the target latency profile, a reproducible first-divergence report, resynchronization behavior under packet loss and reordering, soak tests over the expected match duration, native graphical smoke tests on release hardware, and a documented recovery policy for unsupported divergence.

## Current branch evidence

The competitive work is published on [`blueprint-foundation`](https://github.com/robert-sarah/rbfx-blueprint/tree/blueprint-foundation). The latest published commits include rollback diagnostics and resynchronization, bounded prediction and desync validation, the executable 1v1 rollback demonstration, deterministic time-machine replay persistence, workspace-aware RbScript tooling, and the 2D/3D Scene workspace parity improvements.

The repository keeps a deliberately conservative maturity statement. Passing engine tests proves the contracts exercised by those tests; it does not certify every game genre, GPU driver, operating system, network topology, or external middleware combination. The next useful evidence is therefore not another broad feature list, but a real sample game, a repeatable latency/loss test matrix, and native platform runs recorded as CI artifacts.

## Related documentation

| Topic | Documentation |
| --- | --- |
| Competitive rollback integration | [`CompetitiveNetcode.md`](CompetitiveNetcode.md) |
| Deterministic temporal debugging | [`DeterministicTimeMachine.md`](DeterministicTimeMachine.md) |
| RbScript workspace tooling | [`RbScriptLsp.md`](RbScriptLsp.md) |
| 2D and 3D Scene profiles | [`SceneView2D.md`](SceneView2D.md) |
| Production readiness gates | [`ProductionReadiness.md`](ProductionReadiness.md) |
| Runtime packaging | [`RuntimePackaging.md`](RuntimePackaging.md) |

## References

1. [`CompetitiveNetcode.md`](CompetitiveNetcode.md)
2. [`DeterministicTimeMachine.md`](DeterministicTimeMachine.md)
3. [`RbScriptLsp.md`](RbScriptLsp.md)
4. [`SceneView2D.md`](SceneView2D.md)
5. [`ProductionReadiness.md`](ProductionReadiness.md)
