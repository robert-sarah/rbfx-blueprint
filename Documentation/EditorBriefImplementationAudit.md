# Editor Brief Implementation Audit

## Scope

This audit consolidates the three attached editor briefs: shared visual consistency, RbScript editing workflow, and dock/inspector/resource/console ergonomics. The implementation target is rbfx-blueprint on `blueprint-foundation`; the changes must remain testable under the existing C++17, ImGui, Catch2 and editor build configuration.

## Findings confirmed in the current source

The editor already has a global `EditorDesignSystem` with dark, light and high-contrast palettes, common rounding, spacing and docking colors. However, feature tabs still contain local `IM_COL32` literals, so the global palette does not yet provide a single source for canvas, node, pin and semantic editor colors.

`BlueprintTab::FindPinAt` compares graph-space coordinates against a fixed `12.0f` radius even though the pointer is converted through the current zoom. This makes pin hit testing inconsistent at high zoom. The Blueprint canvas also uses orthogonal polylines, which are valid deterministic routes but can be complemented by a user-facing smooth cable mode without removing the backward-link detour behavior.

`RbScriptTab` already tokenizes the active source, classifies syntax categories, stores diagnostics and reflection data, and supports source snapshots. Its current editing surface remains `InputTextMultiline`, while the colored token renderer is a separate preview. Its completion data is presented in a permanent collapsible panel and does not yet expose completion kinds, context ordering, keyboard selection or a contextual help surface.

The resource browser already persists its selected root, path, filter and selection, but does not persist navigation history or favorites. `InspectorTab` is a forwarding host; category grouping and filtering must therefore be implemented in the active inspector widgets rather than in the host tab. The global console subsystem is the correct location for message deduplication and severity counters, while `ConsoleTab` should expose the controls.

## Implementation plan

The work is split into small, verifiable contracts. First, a shared `EditorTheme` value layer will extend the existing design system with semantic colors and metrics, and Blueprint will use it for the high-impact canvas, pin and node colors. The zoom-aware pin hit radius will be fixed and covered by a deterministic helper test.

Second, RbScript will gain a reusable completion model with explicit kinds and source ranking, a keyboard-navigable popup, and contextual help derived from the existing reflection registry. The colored renderer will be integrated into the editor surface through a stable overlay approach around the native input widget, preserving reliable text editing while eliminating the separate preview-only workflow. The legacy preview remains available as an optional diagnostic mode rather than the only place where color is visible.

Third, the resource browser will gain breadcrumb navigation, back/forward history and persisted favorites. The console will gain message grouping and severity counters. Inspector category/filter behavior will be added at the closest active inspector source where the existing property data is available. A lightweight dock-layout contract will document named slots and presets without falsely claiming that arbitrary native windows can be detached when the current application shell does not provide that capability.

## Implemented in this change

The shared `EditorTheme` semantic layer is now consumed by Blueprint for canvas, node, pin, cable, comment, minimap and diagnostic colors. `BlueprintTab::FindPinAt` now scales its hit radius with graph zoom, preserving deterministic cable routing while making interaction consistent across zoom levels.

`RbScriptCompletion` is now a reusable C++17 completion contract with explicit completion kinds, local-versus-global scope ranking, case-insensitive prefix and substring matching, deterministic ordering and bounded results. `RbScriptTab` consumes the contract for its contextual popup and reflection-based help while retaining the native ImGui editing path and the existing token preview for reliable input behavior. The new deterministic tests cover scope ranking, exact/prefix ordering, case-insensitive search and result limits.

`ResourceBrowserTab` now provides back/forward navigation, breadcrumbs, favorite folders and INI persistence. `Console` now groups consecutive identical messages with repeat counts, maintains severity counters across ring-buffer eviction, and exposes the controls through `ConsoleTab` with persisted settings. The existing workspace, docking and inspector contracts remain the source of truth; no unsupported claim of arbitrary native-window detachment was added.

The editor configuration compiled successfully through `EditorLibrary` and `Tests`. The coherent editor CTest run completed with **418/418 tests passing** in `256.46 seconds`.

## Boundaries

The implementation does not copy Godot branding, source code, exact colors or class names. It does not claim a full IDE-grade text editor: the underlying ImGui input widget remains the reliable editing surface, while the completion and syntax assistance are integrated around it. Native graphical certification on Windows and macOS remains a separate release gate from Linux compilation and CTest evidence. Features from the briefs that require production backends not present in this repository, such as a full LSP transport, real-time multi-user merge service, or platform-specific native GUI certification, remain documented as future work rather than simulated.
