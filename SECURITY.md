# Security Policy

## Supported versions

Security fixes are applied to the latest release branch and the active development branch. The current development line is `blueprint-foundation`; release support is documented in the corresponding GitHub Release notes.

## Reporting a vulnerability

Do not publish exploitable vulnerabilities in a public issue. Open a private GitHub security advisory for `robert-sarah/rbfx-blueprint` or contact the maintainers through the private security channel configured for the repository. Include the affected commit or release, platform, build configuration, minimal reproduction, impact and any logs that do not contain credentials or personal data.

The maintainers will acknowledge a valid report, reproduce it in an isolated environment, classify its severity, prepare a fix, add a regression test and publish a coordinated advisory when appropriate. Do not include access tokens, private project files, crash dumps containing personal paths or proprietary assets in a report.

## Release security expectations

Release artifacts should include dependency inventories, license notices, checksums and provenance. Where signing infrastructure is available, Windows, macOS and Linux artifacts should be signed. Third-party dependencies must be reviewed for license compatibility and known vulnerabilities before a release is marked production-ready.

Crash reporting must be opt-in or disabled by default. Diagnostics should redact user paths, credentials, tokens and project-private content before being uploaded or attached to an issue.

## Scope

This policy covers the engine, editor, Player, World Fabric runtime, Blueprint and rbscript tooling, build scripts and official release workflows. Vulnerabilities in user projects, third-party plugins or external services should be reported to their respective maintainers as well.
