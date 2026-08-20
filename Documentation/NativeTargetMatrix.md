# Native Target and Texture Compression Matrix

## Scope

`PlatformExportAdapter` is the validation boundary between a reproducible `PackageBuildProfile` and a platform export backend. A profile is exportable only when its **platform**, **architecture**, and **texture compression** are supported together. The selected compression is serialized in the build profile and package manifest and participates in the canonical provenance digest.

This matrix describes the engine’s packaging contract. It is not a claim that the Linux development runner has performed native graphical certification on every target. Native certification still requires the corresponding compiler, SDK, display session, GPU driver, signing configuration, and release hardware.

## Supported target matrix

| Platform | Supported architectures | Supported texture compression | Notes |
| --- | --- | --- | --- |
| Linux | `x64`, `arm64` | `None`, `BC1`, `BC3`, `BC5`, `BC7` | Desktop target with threads, GPU, networking, AOT, and dynamic code enabled in the export capability contract |
| Windows | `x64`, `arm64` | `None`, `BC1`, `BC3`, `BC5`, `BC7` | Desktop target with threads, GPU, networking, AOT, and dynamic code enabled in the export capability contract |
| macOS | `x64`, `arm64`, `universal` | `None`, `BC7`, `ASTC` | `universal` denotes a fat desktop artifact assembled from the supported desktop architectures |
| WebAssembly | `wasm32` | `None`, `BC7`, `ASTC` | Threads and dynamic code are disabled by the capability contract |
| Android | `arm64-v8a`, `armeabi-v7a`, `x86_64` | `None`, `ASTC`, `ETC2` | Dynamic code is disabled; the adapter is available for package validation even when the Android SDK is not installed on the host |
| iOS | `arm64` | `None`, `ASTC`, `ETC2` | Dynamic code is disabled; the adapter is available for package validation even when the iOS SDK is not installed on the host |

`None` is intentionally supported on every target. It represents an uncompressed or externally-compressed asset path and allows the package contract to validate before a project chooses a GPU-specific texture format.

## Validation behavior

`PackageBuilder::ValidateProfile` performs the generic profile checks and then delegates target compatibility to `PlatformExportAdapter::Validate`. The adapter rejects an empty output path, an unsupported architecture, or an unsupported texture compression profile. `PackageBuildProfile::FromJSON` therefore rejects invalid target combinations before a package manifest or cache lookup can be produced.

`PackageBuilder::BuildManifest` propagates the resolved compression to `PackageManifest`. `PackageManifest::ToJSON` and `PackageBuildProfile::ToJSON` emit the symbolic compression name, while `PackageManifest::ComputeDigest` mixes it into the canonical FNV-1a payload. Changing the compression consequently invalidates the package provenance identity rather than silently reusing an incompatible artifact.

The `Describe` API exposes comma-separated architecture and compression lists to Blueprint, editor panels, and package dashboards. Android and iOS are first-class adapters; they no longer return an unsupported-platform result merely because their SDKs are unavailable on the current Linux build host.

## Reproducible checks

The target matrix is covered by tests that accept valid Linux, Windows, macOS, WebAssembly, Android, and iOS combinations and reject incompatible examples such as WebAssembly `x64`, Windows `ASTC`, Android `x64`, and iOS `BC7`. The full coherent Linux run currently reports **415/415 tests passing**.

For a real release, the matrix must be followed by native gates: compile and link on the target toolchain, launch a graphical smoke test where applicable, cook representative assets, validate the produced package on release hardware, and record SDK/compiler/GPU metadata in the build provenance.

## Compatibility policy

The matrix is intentionally conservative. Adding a new architecture or compression requires a capability update, a serialization round-trip test, a valid/invalid matrix test, a manifest-digest test, and a native CI or release-gate plan. Unsupported combinations fail closed with a diagnostic that names the rejected architecture or compression profile.

The contract does not emulate a platform, fake a native SDK, or claim a successful package launch without the corresponding native validation evidence.
