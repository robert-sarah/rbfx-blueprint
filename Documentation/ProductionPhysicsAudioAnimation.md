# Production Physics, Audio and Animation

## Scope

This document describes the production contracts added for competitive gameplay. The implementation is deliberately split between deterministic engine-side decisions and platform-specific execution. The former is covered by automated tests; the latter remains the responsibility of the selected physics, audio and animation backend.

## Physics production profile

`PhysicsProductionProfile` validates a project-wide fixed-step contract before a physics world starts. It rejects non-finite or unsafe time steps, invalid substep counts, zero solver budgets, and zero body/contact capacities. Optional capabilities are declared explicitly with `PhysicsProductionFeature`: destruction, cloth, soft body, fluid, vehicle and ragdoll.

The profile is not a fake solver. It does not claim to implement cloth, fluid or destruction dynamics. Instead, it gives a deterministic gate that prevents a project from silently enabling an unsupported capability. A backend adapter can inspect `EnabledFeatures` and either bind the corresponding solver or fail during project validation.

## Spatial audio

`AudioSpatializer` evaluates the stable source-to-listener terms needed by an audio backend. It provides:

| Term | Contract |
|---|---|
| Distance attenuation | Smooth bounded attenuation between `minDistance` and `maxDistance`. |
| Pan | Signed projection on the listener right vector, clamped to `[-1, 1]`. |
| Stereo gains | Constant-power left and right gains derived from pan and source volume. |
| Audibility | False when the source is disabled or outside its audible range. |

The evaluator is deterministic and backend-neutral. HRTF convolution, reverb sends, occlusion rays, propagation delay and Wwise/FMOD device integration still belong to the runtime audio backend. This boundary is intentional: the engine owns reproducible spatial terms while the platform owns DSP execution.

## Animation layers

`ProductionAnimationLayerStack` complements `AnimationStateMachine`, `AnimationBlendSpace` and `Sequencer`. It evaluates enabled base layers in stable lexical order, normalizes their weights to one, and emits additive layers afterward without normalization. Invalid names, empty clips and negative weights are rejected; authored weights are bounded to the normalized range.

The stack emits clip contributions rather than bone poses. The existing animation backend consumes those contributions to evaluate skeletal, sprite or procedural poses. This keeps the layer contract usable for both 2D and 3D profiles while avoiding a second, incompatible pose representation.

## Validation

`TestPhysicsAudioAnimationProduction.cpp` proves that stereo spatialization remains stable, disabled or out-of-range sources are inaudible, physics settings reject unsafe fixed-step budgets, optional capabilities remain inspectable, and animation layer weights converge deterministically. Existing animation state-machine, blend-space, sequencer, audio mixer and physics-world tests continue to cover their native runtime behavior.

The remaining production work is platform certification: backend solver coverage, visual cloth/destruction authoring, device-specific HRTF and reverb, motion matching data pipelines, facial capture, control rigs and long-duration performance tests must be validated with the actual target integrations.

