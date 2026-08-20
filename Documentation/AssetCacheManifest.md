# Reproducible Asset Cache Manifest

## Purpose

The asset pipeline cache manifest records the output of a deterministic asset build without storing compiled artifacts in source control. It is designed to be persisted beside a build graph, transferred to a remote cache, or audited as part of a reproducible package build.

The manifest is represented by `AssetCacheManifest` in `AssetPipeline`. Each entry identifies one logical asset, its content digest, the build revision that produced it, its dependency list and the cache artifact path.

## Entry contract

| Field | Meaning |
| --- | --- |
| `asset` | Stable logical asset identifier. It must be non-empty and unique. |
| `contentDigest` | Digest of the source content and relevant importer inputs. |
| `buildRevision` | Revision or toolchain fingerprint used to produce the cached result. |
| `dependencies` | Logical assets consumed by this result. The list is normalized into stable order. |
| `artifact` | Relative path or content-addressed key of the cached output. |

An entry with an empty asset identifier, content digest, build revision or artifact is rejected. Duplicate asset identifiers are rejected during deserialization, and duplicate dependencies inside one entry are rejected as malformed input rather than silently collapsed.

## JSON persistence

`AssetCacheManifest::ToJSON` emits the manifest in deterministic entry order. `FromJSON` validates the root object, entry array, required fields, dependency arrays and duplicate keys before replacing the destination manifest. A failed import leaves the destination unchanged, which makes the method safe to use for editor reload, crash recovery and build-graph transactions.

The persistence contract is intentionally independent from the physical cache backend. A local filesystem cache, a distributed object store or a CI artifact service can use the same manifest while applying its own transport and retention policy.

## Reproducibility workflow

A production build should compute its source and toolchain inputs first, resolve dependencies in stable order, and then write the resulting manifest only after all entries have been validated. A later build can compare manifests before downloading artifacts:

1. Compare the logical asset identifier and content digest.
2. Compare the build revision and importer/toolchain fingerprint.
3. Compare the normalized dependency list.
4. Reuse the artifact only when all relevant fields agree.
5. Rebuild and replace the entry when an input differs.

This procedure prevents a stale artifact from being accepted only because its filename is unchanged.

## Evidence and limits

The `TestAssetPipeline.cpp` coverage verifies round-trip persistence, stable ordering and rejection of malformed or duplicated entries. The manifest is a production contract, not a complete FBX importer, texture cooker, LOD generator or distributed cache service. Those systems can be connected to the manifest without changing its validation rules.

The next extensions are content-addressed artifact transfer, dependency graph invalidation, importer version fingerprints, remote cache leases and build provenance links to `SemanticBuildCapsule`.
