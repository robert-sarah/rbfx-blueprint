# Model Import Profile

## Purpose

`ModelImportProfile` is the deterministic contract applied before a model importer or cooker consumes a mesh or scene source. It does not parse FBX, glTF, OBJ, or DAE itself. Instead, it makes the decisions that affect conversion explicit, versioned, serializable, and stable across machines.

> A valid profile is a prerequisite for reproducible model cooking; it is not a claim that every source format is parsed by the same native backend.

## Supported profile fields

| Field | Contract |
|---|---|
| `version` | Currently `1`; other versions are rejected. |
| `sourceFormat` | Case-insensitive `gltf`, `glb`, `obj`, `fbx`, or `dae`. |
| `upAxis` | `X`, `Y`, or `Z`. |
| `handedness` | `Left` or `Right`. |
| `unitsPerMeter` | Finite value in `(0, 1000]`. |
| `generateTangents` | Whether tangent data is generated when the source does not provide it. |
| `generateLightmapUV` | Whether a lightmap UV set is requested. |
| `uvChannelCount` | Integer from `1` through `8`. |
| `lodScreenSizes` | One to sixteen finite, strictly descending thresholds in `(0, 1]`. |
| `provenance` | Non-empty source or build provenance string, at most 256 characters. |

## Determinism

The JSON representation is canonical with a normalized lower-case source format and stable field names. The profile digest includes the version, normalized format, coordinate conventions, quantized unit scale, generation flags, UV count, quantized LOD thresholds, and provenance. Floating-point values are quantized to one-millionth before hashing so insignificant binary representation differences do not alter the profile identity.

The profile digest participates in the asset import settings hash. A change to units, axes, tangent generation, UV policy, LOD thresholds, or provenance therefore creates a distinct cooked variant instead of silently reusing an incompatible cache entry.

## Validation behavior

Validation is intentionally filesystem- and backend-independent. It rejects unsupported formats, invalid coordinate enum values during JSON restoration, non-finite or out-of-range unit scales, invalid UV counts, unordered or out-of-range LOD thresholds, empty provenance, incomplete JSON schemas, and unknown profile versions.

The contract is tested by `TestModelImportProfile.cpp`, including a round trip through JSON, case normalization for `FBX` and `GLTF`, rejection of unsafe production values, and digest changes when provenance changes.

## Scope and limits

This contract does not replace a format parser, geometry optimizer, tangent generator, UV packer, or platform-specific importer. Those systems remain responsible for reading source data and implementing the requested operations. The profile provides the stable input contract and cache identity they must honor.
