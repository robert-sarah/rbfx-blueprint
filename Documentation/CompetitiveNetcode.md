# Competitive Netcode

## Purpose

`rbfx-blueprint` provides a transport-independent rollback contract for competitive indie and AA multiplayer games. The engine does not prescribe a socket protocol, matchmaking service, or replication topology. Instead, `RollbackManager` owns the deterministic state history and input replay contract while the existing `Network`, `Replica`, RPC, and snapshot layers carry inputs, authoritative checkpoints, and digest evidence.

The intended architecture is deliberately narrow: a fixed-step gameplay simulation produces the same state for the same initial state and input stream, the local client predicts a bounded number of frames ahead, and an authoritative checkpoint can replace the local state without coupling rollback code to a particular transport.

## Core contract

| Contract | Responsibility | Production rule |
| --- | --- | --- |
| `RecordInput` | Retain one input packet per simulation frame | Packets are replaced by frame and retained in ascending frame order |
| `SaveState` | Retain a state checkpoint and its deterministic digest | History is bounded by `RollbackManager::GetCapacity()` |
| `SetPredictionWindow` | Limit local simulation lead over the latest authority | The game loop must call `IsPredictionAllowed` before advancing a speculative frame |
| `Reconcile` | Apply authority and replay retained later inputs | The simulator callback must be deterministic and side-effect controlled |
| `ApplyResynchronization` | Validate and apply a transport-independent checkpoint | A checkpoint with an invalid supplied digest is rejected before state application |
| `ValidateAuthoritativeDigests` | Compare authoritative frame digests with predicted history | The first mismatch is exposed through `RollbackDiagnostics::firstDivergentFrame` |

## Fixed-step client loop

The simulation loop should use one integer `NetworkFrame` for gameplay progression. Rendering, audio presentation, and UI interpolation must not mutate the deterministic state directly.

```cpp
RollbackManager rollback(240);
rollback.SetPredictionWindow(8);

for (;;)
{
    const NetworkFrame nextFrame = simulationFrame + static_cast<NetworkFrame>(1);
    if (!rollback.IsPredictionAllowed(nextFrame))
        break; // Wait for authority or transport progress.

    RollbackInput input;
    input.frame = nextFrame;
    input.values["moveX"] = localMoveX;
    input.values["jump"] = localJump;
    rollback.RecordInput(input);

    StringVariantMap state = ReadDeterministicGameplayState();
    if (!ApplyGameplayInput(input.values, state))
        break;

    rollback.SaveState(nextFrame, state);
    WriteDeterministicGameplayState(state);
    simulationFrame = nextFrame;
}
```

`RecordInput` does not send data and does not own a socket. The owning network system should serialize the input with its frame number, transmit it to the authority, and deliver the authority’s checkpoint back to the simulation owner.

## Authoritative reconciliation

When a server or host sends a checkpoint, the receiver should include the authoritative frame, the authoritative state, and the digest computed by the authority. The digest is optional only when the transport cannot carry one; in that case `ApplyResynchronization` computes a local digest for the checkpoint state.

```cpp
RollbackResynchronization checkpoint;
checkpoint.frame = packet.frame;
checkpoint.digest = packet.digest;
checkpoint.state = DecodeState(packet.payload);

StringVariantMap correctedState;
const bool accepted = rollback.ApplyResynchronization(
    checkpoint,
    [](const StringVariantMap& input, StringVariantMap& state)
    {
        return SimulateOneDeterministicFrame(input, state);
    },
    correctedState);

const RollbackDiagnostics& diagnostics = rollback.GetLastDiagnostics();
if (!accepted || diagnostics.rejectedInputs != 0)
{
    RequestAuthoritativeResynchronization();
    return;
}

WriteDeterministicGameplayState(correctedState);
simulationFrame = checkpoint.frame + static_cast<NetworkFrame>(diagnostics.replayedInputs);
```

The callback is intentionally small. It should advance only one deterministic simulation frame. It must not perform network sends, read wall-clock time, allocate nondeterministic identifiers, query presentation-only objects, or invoke an external side effect that cannot be replayed.

## Prediction window and history sizing

The prediction window is a gameplay policy, not a latency estimate hidden inside the engine. A typical competitive prototype starts with 6–8 frames and tunes the value using measured round-trip time, input sampling rate, and the maximum rollback budget of the game. The state and input capacities must cover the prediction window plus the worst-case authoritative delivery delay and an operational margin.

`GetPredictionDepth(frame)` returns the number of frames between a candidate frame and the latest authoritative frame. `IsPredictionAllowed(frame)` returns `false` when the candidate would exceed the configured window. A game may choose to stall, display a netcode warning, or request a checkpoint when the window is exhausted; it should not silently continue unbounded prediction.

## Desync detection

`SaveState` stores a canonical digest alongside each retained state. The digest sorts state keys and hashes each key, variant type, and canonical `Variant::ToString()` representation using the same FNV-1a seed used by the deterministic simulation services. This makes insertion order irrelevant while still exposing state changes.

A host can periodically send a compact digest stream without sending a full state. The receiver compares it against the bounded predicted history:

```cpp
ea::vector<RollbackDigestSample> authoritativeDigests;
for (const NetworkDigestPacket& packet : packets)
    authoritativeDigests.push_back({packet.frame, packet.digest});

if (!rollback.ValidateAuthoritativeDigests(authoritativeDigests))
{
    const RollbackDiagnostics& diagnostics = rollback.GetLastDiagnostics();
    Log::Write(LOG_ERROR,
        "Deterministic desync at frame " + String(static_cast<long long>(diagnostics.firstDivergentFrame)));
    RequestAuthoritativeResynchronization();
}
```

`RollbackDiagnostics` distinguishes replay work from divergence evidence. `predictedInputs` is the number of retained inputs after the authoritative frame, `replayedInputs` is the number successfully passed through the simulator, `rejectedInputs` counts callback failures, `comparedDigests` counts digest comparisons, and `firstDivergentFrame` identifies the earliest mismatch observed in the submitted ordered batch.

## Transport integration boundary

The existing `Network` and `Replica` layers should treat rollback as a deterministic gameplay service rather than a second replication system. A practical packet flow is:

| Direction | Payload | Consumer |
| --- | --- | --- |
| Client → server | Input frame, player identifier, input values, sequence metadata | Server simulation and validation |
| Server → clients | Authoritative checkpoint frame, state payload, state digest | `ApplyResynchronization` |
| Server → clients | Optional digest samples for recent frames | `ValidateAuthoritativeDigests` |
| Client → server | Resynchronization request with local diagnostics | Server checkpoint builder and telemetry |

RPC can carry control messages such as a resynchronization request, while gameplay inputs should remain in a frame-indexed input stream that can be buffered, validated, and replayed. Relevancy filtering may reduce replicated object state, but every object that influences the deterministic match must be represented in the checkpoint or reconstructed from deterministic match inputs.

## Deterministic simulation checklist

Before using rollback in a production match, the gameplay module should satisfy the following requirements:

1. The simulation advances from an integer frame and a fixed timestep.
2. All random streams are explicitly seeded and serialized as part of the deterministic state.
3. Physics, hit detection, timers, animation gameplay events, and AI decisions consume simulation time rather than wall-clock time.
4. Input packets are immutable after recording and are identified by frame.
5. The simulator callback is safe to call repeatedly during replay.
6. State digests are emitted in development and continuous-integration builds.
7. A mismatched digest triggers a reproducible checkpoint request rather than being hidden by visual correction.
8. Replays persist the initial state, match configuration, and complete frame-indexed input stream.

Rollback cannot make a nondeterministic gameplay module deterministic by itself. If a module reads an unsynchronized clock, depends on unordered iteration, or performs a non-replayable side effect, the diagnostic layer will correctly report divergence but cannot infer the missing gameplay contract.

## Operational diagnostics

A competitive build should record the following values per match interval: latest authoritative frame, local simulation frame, prediction depth, maximum observed rollback distance, replayed input count, rejected input count, digest comparison count, and first divergent frame. These values are intended for an in-game netgraph, automated soak tests, and post-match telemetry. They should be sampled rather than logged every frame in a release build.

The current implementation is transport-independent and production-testable. It does not claim to implement matchmaking, NAT traversal, encryption, anti-cheat, server hosting, or a complete production protocol. Those services remain integration responsibilities above the deterministic rollback contract.

## Related APIs

- `Source/Urho3D/Replica/RollbackManager.h`
- `Source/Urho3D/Replica/SnapshotBuffer.h`
- `Source/Urho3D/WorldFabric/DeterministicSimulation.h`
- `Source/Urho3D/WorldFabric/UniversalDeterministicTimeMachine.h`
- `Source/Tests/TestNetworkProduction.cpp`
