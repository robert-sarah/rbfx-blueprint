# Reproducible Package Manifest

`PackageManifest` is the deterministic record emitted by `PackageBuilder` for a cooked package. It complements the package profile with the exact recipe, cache input, artifact identity, and provenance needed to audit or reproduce a build.

## Manifest-level provenance

A `PackageBuildProfile` may carry two upstream identifiers:

| Field | Meaning |
| --- | --- |
| `buildGraphDigest` | Digest of the ordered BuildGraph recipe, including LOD, texture-cooking, and provenance tasks. |
| `assetCacheDigest` | Digest of the content-addressed asset-cache manifest used as input. |

`PackageManifest` persists these values together with `worldFabricDigest`. During `BuildManifest`, the manifest computes a canonical `provenanceDigest` after validation. The digest does not include its own stored value, so it remains stable across JSON round-trips.

## Per-file provenance

Each `PackageFileEntry` can record the following optional values:

| Field | Meaning |
| --- | --- |
| `contentDigest` | Content-addressed digest of the cooked artifact. |
| `importProfileDigest` | Digest of the model, texture, shader, or script import profile that produced it. |
| `provenance` | Stable human-readable recipe label such as `BuildGraph/CookModel/arena`. |

Existing manifests remain compatible: these fields are optional when loading older version-1 JSON manifests. New manifests always serialize them, including empty strings when no producer supplied a value.

## Canonical digest rules

`PackageManifest::ComputeDigest()` validates the manifest first, sorts files by `packagePath` and then `sourcePath`, and mixes every identity field with an explicit delimiter. The digest therefore does not depend on candidate insertion order. It includes the package version, profile name, platform, architecture, World Fabric digest, BuildGraph digest, asset-cache digest, and every per-file hash, size, profile digest, and provenance label.

> A matching `provenanceDigest` proves that the manifest metadata is identical; it does not prove that an external artifact exists or that a platform toolchain produced byte-identical binaries unless those artifact digests and toolchain inputs are also recorded.

## Validation and compatibility

`PackageBuilder::ValidateManifest` continues to reject duplicate package paths, invalid paths, empty profiles, and unsupported platform metadata. Provenance fields do not weaken those checks. The JSON loader accepts manifests produced before provenance support and defaults absent fields to empty values, preserving version-1 compatibility.

The corresponding tests cover JSON round-trip, recipe and cache identifiers, per-file provenance, content and import-profile digests, and insertion-order-independent canonical digests.
