# Universal Deterministic Time Machine

## Role in competitive production

`UniversalDeterministicTimeMachine` is the temporal debugging and replay service for `rbfx-blueprint`. It retains deterministic frames across gameplay, physics, AI, network, Blueprint, RbScript, and custom domains. The service is intentionally independent of sockets and rendering: it records state, input, fixed-step metadata, branch generations, labels, and digests so a divergence can be reproduced and inspected without relying on the live match.

> A replay is valid only when the same initial state, fixed delta, frame-indexed inputs, and deterministic step function produce the same state digest at every retained frame.

The machine is a bounded history. It is not a general-purpose save-game format and it does not persist arbitrary engine resources, GPU state, operating-system time, or external side effects.

## Frame lifecycle

| Operation | Effect | Failure conditions |
| --- | --- | --- |
| `Start` | Creates the initial frame `0` on a named branch | The branch is reset and previous history is discarded |
| `Advance` | Runs one deterministic step and appends a frame | No active branch, missing step, or failed step callback |
| `Restore` | Moves the current state cursor to a retained frame | The frame is no longer inside the bounded history |
| `ReplayTo` | Reapplies recorded inputs from the current cursor | A source input frame is missing or the step callback fails |
| `CreateBranch` | Copies frames through a checkpoint and switches to the new branch | Empty/duplicate branch or missing checkpoint |
| `SwitchBranch` | Moves to the last retained frame of another branch | Branch does not exist or contains no frames |
| `CompareFrames` | Reports digest and changed-state-key differences | One of the requested frames is absent |
| `FindFirstDivergence` | Finds the earliest differing frame across two branches | A branch does not exist |

A successful `Advance` increments the internal generation, stores the fixed-step input, computes a canonical state digest, and trims the branch to its configured capacity. Branches are copied at the selected checkpoint, which makes alternate gameplay decisions independently inspectable.

## Deterministic step contract

The step callback receives the frame number, fixed delta, input map, current state, and an output state. It should compute exactly one simulation frame and return `false` when the frame cannot be produced.

```cpp
UniversalDeterministicTimeMachine machine(256);
StringVariantMap initialState;
initialState["positionX"] = Variant(0);
initialState["velocityX"] = Variant(0);
REQUIRE(machine.Start(initialState, "main", DeterministicTimeMachineDomain::Gameplay));

const UniversalDeterministicStep step = [](unsigned frame, float fixedDelta,
    const StringVariantMap& input, const StringVariantMap& current, StringVariantMap& next)
{
    next = current;
    next["positionX"] = current.at("positionX").GetInt() + input.at("moveX").GetInt();
    next["velocityX"] = input.at("moveX").GetInt();
    return fixedDelta > 0.0f && frame > 0;
};

StringVariantMap input;
input["moveX"] = Variant(1);
if (!machine.Advance(input, step))
    HandleDeterministicSimulationFailure();
```

The callback must not read wall-clock time, use an unseeded random source, depend on unordered iteration, or emit a non-replayable side effect. Audio, particles, camera presentation, network sends, and editor notifications should be driven from the resulting frame or from explicitly recorded events rather than created as hidden simulation inputs.

## Branches and temporal debugging

A branch is an alternate retained history beginning at a checkpoint. The main branch remains available after `CreateBranch`, while the current cursor switches to the new branch. Branch names are sorted for digest and JSON output, making exported replays stable across unordered-map iteration.

```cpp
REQUIRE(machine.CreateBranch("rollback-investigation", 120));
REQUIRE(machine.Advance(input, step, DeterministicTimeMachineDomain::Network, "authoritative correction"));

unsigned divergenceFrame = 0;
DeterministicFrameDifference difference;
if (machine.FindFirstDivergence("main", "rollback-investigation", divergenceFrame, difference))
{
    Log::Write(LOG_ERROR, "First divergent frame: " + String(divergenceFrame));
    for (const std::string& key : difference.changedKeys)
        Log::Write(LOG_ERROR, "Changed key: " + key);
}
```

`CompareFrames` reports the two digests and a sorted list of state keys whose type or canonical value changed. `FindFirstDivergence` examines the union of retained frame numbers and treats a frame that exists on only one branch as a divergence. This is useful for rollback investigations, AI decision auditing, and Blueprint/RbScript execution comparisons.

## JSON replay persistence

`ExportReplay` serializes the complete bounded history to UTF-8 JSON. The output contains the fixed delta, capacity, current cursor, current branch, current state, all branch names, and every retained frame. Each frame includes its generation, domain, state, input, label, and digest.

```cpp
const std::string jsonReplay = machine.ExportReplay();
WriteFile("match-2026-rollback.rbreplay", jsonReplay);

UniversalDeterministicTimeMachine restored;
std::string error;
if (!restored.ImportReplay(jsonReplay, &error))
    Log::Write(LOG_ERROR, "Replay import failed: " + error);
```

Digest values are encoded as decimal strings rather than JSON numbers so the complete unsigned 64-bit value is preserved on every platform. The JSON exporter iterates branch names in sorted order and retains frames in ascending order, producing reproducible artifacts suitable for CI, bug reports, and review.

The importer validates the document before changing the machine. It checks the version, positive fixed delta, minimum capacity, branch uniqueness, frame ordering, valid domain values, state-map shape, and every frame digest. It also verifies that the serialized current state matches the serialized current frame. If any validation fails, the existing machine remains unchanged.

A minimal replay shape is shown below. The actual `state` and `input` objects are generated through rbfx `JSONValue::SetStringVariantMap`, which preserves the variant type metadata required for deterministic restoration.

```json
{
  "version": 1,
  "fixedDelta": 0.0166666675,
  "capacity": 256,
  "currentFrame": 120,
  "generation": 123,
  "currentBranch": "main",
  "state": { "positionX": { "type": 3, "value": 12 } },
  "branches": [
    {
      "name": "main",
      "frames": [
        {
          "frame": 0,
          "generation": 1,
          "domain": 0,
          "digest": "1469598103934665603",
          "label": "Initial state",
          "state": {},
          "input": {}
        }
      ]
    }
  ]
}
```

The example is illustrative rather than a complete replay: a production artifact contains all retained frames and the exact digest generated from each state. Hand-editing a digest is expected to make `ImportReplay` fail, which prevents a corrupted replay from silently entering a debugging session.

## Replay-driven regression testing

A production test should record the initial state and all frame inputs, run the simulation once, export the replay, import it into a new machine, and replay with the same step function. The final state digest and branch digest must match. A second test should alter one input or state value and require `FindFirstDivergence` to identify the expected frame.

The repository covers these contracts in `TestUniqueProductionServices.cpp`, including a multi-branch round trip, byte-stable re-export, divergence preservation, and atomic rejection of a tampered digest. The replay service complements `DeterministicSimulation` and `RollbackManager`: the former is the fixed-step simulation contract, the latter is the network reconciliation contract, and this service is the temporal inspection and artifact contract.

## Capacity and production limits

Capacity should cover the maximum expected rollback and investigation horizon. Raising the capacity increases retained state memory proportionally to the size of every frame state and input map. A release build should use bounded history and sampled telemetry, while an editor or CI build may use a larger capacity for diagnosis.

The service does not serialize executable callbacks. Import restores data only; the caller must provide the correct deterministic step function for the game, Blueprint graph, or RbScript module. This boundary is deliberate: executable code remains part of the build being investigated, while the replay artifact remains portable and reviewable.

## Related APIs

- `Source/Urho3D/WorldFabric/UniversalDeterministicTimeMachine.h`
- `Source/Urho3D/WorldFabric/DeterministicSimulation.h`
- `Source/Urho3D/Replica/RollbackManager.h`
- `Source/Tests/TestUniqueProductionServices.cpp`
