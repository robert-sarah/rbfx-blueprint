# Blueprint and rbscript editor workflow

## Blueprint interactions

The Blueprint canvas defers context-menu opening to the next UI frame. This keeps a right-click menu open while selection and canvas ownership are updated. Destructive deletion is routed through a confirmation modal and is recorded as an undoable graph snapshot.

Blueprint graph revisions are retained in a bounded history of 32 snapshots. The history panel supports restoration, selecting two revisions for a structural diff, and a conservative non-conflicting merge that preserves the base version when node edits conflict. Graph snapshots are also included in the editor undo/redo stack.

The Blueprint toolbar includes graph validation, execution, JSON export, automatic layout, minimap and comment visibility, history, and runtime watch controls. The canvas provides node search highlighting, reflection-backed node tooltips, double-click fit-to-view, graph statistics, breakpoint markers, and last-executed pin values. Zoom, pan, panel visibility, and pin-value visibility are persisted through the editor INI settings.

Blueprint assets can be selected from the resource browser, opened in the Blueprint tab, and removed through the project resource request system so the browser, active editor tab, and filesystem state remain synchronized.

## rbscript files

The resource browser registers the `rbscript` resource factory. From the rbscript tab, **New rbscript** creates a unique source file under `Data/Scripts/` without overwriting an existing resource. Four templates are available: **Empty**, **Component**, **Gameplay**, and **Network**. Every template is checked by the shared rbscript editor contract and uses the typed brace-based rbscript syntax.

The integrated rbscript tab provides the following workflow:

| Command or panel | Behavior |
|---|---|
| New rbscript | Creates a source file from the selected typed template and opens it. |
| Open Browser | Activates the resource browser so an existing `.rbscript` file can be selected. |
| Compile | Runs the active source through the editor compiler path. |
| Save | Writes the active source to the project `Data/` directory and updates the disk baseline. |
| Auto compile | Recompiles the active document after source edits. |
| Syntax preview | Displays tokenized rbscript with token-aware colors. |
| Diagnostics | Shows lexer, parser, compiler, and runtime diagnostics with source locations. |
| Reflection autocomplete | Offers rbscript keywords and names obtained from the rbfx reflection type and function registries. |
| Symbol outline | Lists scripts, fields, functions, and event handlers from the parsed AST. Selecting a symbol exposes its definition span and rename action. |
| Find and replace | Provides editor-local replacement with an undoable source snapshot. |
| Debugger | Supports breakpoints, step, step-over, continue, stop, call stack, locals, watches, and preservation of breakpoints across recompilation. |

The editor keeps an in-memory document per open resource and parses the AST after tokenization. Symbol spans are retained for navigation and rename operations. Debug refresh preserves breakpoints and records whether the VM state was migrated during hot reload.

## External conflict resolution

The editor maintains the last known disk source for each open resource. When a file watcher reload arrives while the document is dirty and the disk content differs from both the editor source and the previous disk baseline, the tab opens an **rbscript Conflict** modal.

| Conflict action | Result |
|---|---|
| Keep mine | Keeps the in-memory source and adopts the new disk content as the comparison baseline. |
| Use disk | Replaces the in-memory document with the disk source, recompiles it, and clears the dirty state. |
| Show diff | Displays a line-oriented comparison of the editor and disk versions before choosing an action. |

Self-triggered saves are guarded so their reload notification is not misclassified as an external conflict. Failed writes clear the guard and therefore cannot suppress a later legitimate reload.

## Automated contracts and validation

`Source/Tests/TestEditorUI.cpp` provides headless editor contracts for rbscript resource routing, all template sources, Blueprint snapshot restoration and deletion invariants, and save/reload source stability. These tests exercise the same shared template and extension contract used by the editor without requiring a platform GUI.

The Linux editor was rebuilt successfully with `URHO3D_EDITOR=ON`. The full test target now contains **309 passing tests**, including the rbscript editor contracts. The Windows cross-build remains a PE validation step; a real Windows GUI smoke test still requires execution on Windows because the Linux environment cannot launch the Windows editor interface.

## Production scope and known validation boundary

The requested editor features are implemented in the fork: safe Blueprint deletion, graph history and recovery, structural diff and conservative merge, rbscript syntax/token presentation, AST symbols, rename, reflection autocomplete, debugger controls, watches, hot-reload breakpoint preservation, external conflict resolution, resource templates, find/replace, Blueprint search and tooltips, graph export, view persistence, runtime pin values, and editor contracts.

> The remaining validation boundary is environmental rather than an unimplemented editor feature: Linux can compile and test the Windows binaries, but cannot replace a real Windows host for an interactive GUI smoke test. The cross-compiled distribution must therefore be tested once on a Windows machine before being described as GUI-validated on Windows.
