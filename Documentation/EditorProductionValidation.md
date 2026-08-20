# Editor Production Validation

`EditorProductionValidation` is a deterministic, UI-independent contract used by editor smoke tests and project diagnostics. It validates the minimum state required before a production editor session is opened or a reference workflow is executed.

## Validated state

The input contract records the active workspace, the active scene, the selected 2D or 3D scene profile, autosave configuration, pending recovery state, Blueprint readiness, RbScript workspace readiness and asset database readiness.

Exactly one of `sceneView2D` and `sceneView3D` must be enabled. A production session must have an active scene and a ready asset database. Autosave retention is bounded to one hundred snapshots when autosave is enabled, and its directory must be writable. A pending recovery manifest is reported as a warning so the editor can require explicit user review without silently discarding recoverable work.

## Diagnostics

Every diagnostic has a stable severity, code and message. Errors make `passed` false. Warnings preserve a passing report while remaining visible to the editor and CI output. The report also contains a deterministic FNV-1a-style digest of the complete input state, which allows a smoke test or telemetry record to prove which editor configuration was validated.

| Code | Severity | Meaning |
| --- | --- | --- |
| `workspace.empty` | Error | The workspace identifier is empty. |
| `workspace.unknown` | Warning | The identifier is not one of the built-in profiles. |
| `scene.missing` | Error | No active scene is available. |
| `scene.view-mode` | Error | Both or neither 2D and 3D profiles are active. |
| `autosave.directory` | Error | Autosave is enabled but its directory is unavailable. |
| `autosave.retention` | Error | The retention limit is outside the safe range. |
| `autosave.disabled` | Warning | Autosave is disabled for the session. |
| `recovery.pending` | Warning | A recovery manifest requires explicit review. |
| `blueprint.unavailable` | Error | Blueprint services are not ready for the selected workspace. |
| `rbscript.unavailable` | Error | RbScript services are not ready for the selected workspace. |
| `assets.unavailable` | Error | The asset database is not ready. |

## Reference scenarios

The engine exposes stable reference inputs for Scene 2D, Scene 3D, Blueprint, RbScript and Shader Graph workflows. They are intentionally independent of ImGui and can therefore be used by headless CI, editor startup diagnostics and future Windows/macOS smoke runners.

The current contract validates readiness; it does not replace the real scene renderer, resource importers or editor UI. Graph interaction, OS-level window execution and platform-specific filesystem permissions still require platform smoke tests.
