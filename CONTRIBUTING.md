# Contributing to rbfx-blueprint

## Development baseline

rbfx-blueprint is a C++17 engine. Changes should preserve the distinction between engine contracts, platform backends, editor integrations and project-specific content. New public types must avoid collisions with names already defined by rbfx and must use the repository’s ABI-safe conventions.

Before opening a pull request, configure and build the smallest relevant target, run the affected tests and then run the complete Linux suite. The versioned CMake presets are the preferred entry point:

```bash
cmake --preset linux-tests-debug
cmake --build --preset linux-tests-debug
ctest --preset linux-tests-debug --output-on-failure
```

For release evidence:

```bash
cmake --preset linux-tests-release
cmake --build --preset linux-tests-release
ctest --preset linux-tests-release --output-on-failure
```

Do not commit build directories, generated packages, temporary probes, credentials or user-specific paths. Run `git diff --check` and inspect `git status --short` before committing.

## Tests and diagnostics

Every new production contract should have deterministic positive and negative tests. Tests must not depend on a graphics device, network access, wall-clock timing or a user home directory unless the test is explicitly marked as an integration test. Malformed input must return a diagnostic rather than crash. New diagnostics should redact absolute user paths and secrets.

Changes to Blueprint, rbscript, serialization, World Fabric scheduling, resource import, networking, threading or packaging require regression coverage for invalid input, duplicate identifiers, ordering, cancellation and recovery where applicable. Performance changes should include a bounded budget or a documented measurement.

## Pull requests

A pull request should explain the problem, the design, the affected platforms, the tests executed, the known limitations and any migration impact. If a feature depends on native Windows, macOS, GPU, SDK or signing infrastructure, state clearly which evidence was executed and which evidence remains pending.

Keep commits focused and use descriptive messages. Update the relevant documentation, changelog and release notes when a public contract or user-facing behavior changes. Do not claim platform certification from cross-compilation alone.

## Code review checklist

Reviewers should confirm that the change compiles on the supported configurations, does not introduce an ODR collision, preserves deterministic behavior where required, avoids unbounded collections in production contracts, handles errors explicitly, adds tests, updates documentation and leaves the repository free of generated artifacts.
