# Scene View 2D and 3D Parity

`rbfx-blueprint` treats 2D as a first-class authoring profile rather than as a reduced rendering path. The same scene resource, selection model, undo stack, resource reload flow, Blueprint tooling, RbScript tooling, and World Fabric metadata remain available in both profiles.

## 2D profile

Enable the **2D** button in the Scene workspace toolbar. The active scene page switches to an orthographic XY camera, places the camera on the negative Z axis, and preserves the scene resource and selection. Switching back restores the editor's perspective 3D camera profile. The setting is persisted in the editor INI state, while the per-scene authoring controls are stored next to the scene in its `.user.json` configuration.

The 2D toolbar exposes four controls:

| Control | Contract |
| --- | --- |
| Grid | Enables or disables the world-aligned XY overlay. |
| Snap | Enables translation snapping in the XY plane for the active scene page. |
| Grid size | Sets the authored grid spacing in world units. |
| Snap size | Sets the translation snap spacing in world units. |

Grid spacing and snap spacing are clamped to a safe range of `0.1` to `1000` world units when loaded. The grid is rendered from the camera projection, so it remains aligned with scene coordinates when the viewport is resized or moved. Major lines are emphasized every five cells and the world axes are highlighted.

## Transform rules

When 2D mode is active, the transform gizmo is constrained to the XY plane. Translation and scale expose X/Y only; rotation is constrained to the Z axis. If 2D snapping is enabled, translation uses the scene page's snap spacing. Holding the normal editor snap modifier continues to use the global transform-gizmo settings for other operations.

These rules protect the depth axis while keeping the same selection, undo, redo, prefab, and scene-save paths used by 3D editing. A 2D scene can therefore contain 3D-capable nodes and components without creating a separate incompatible asset format.

## Four-pane 3D profile

The four-pane layout remains available for 3D work and provides Perspective, Top, Front, and Right cameras. The 2D profile intentionally selects a single orthographic XY canvas so that mouse interaction, snapping, and the grid share one unambiguous coordinate plane. Switching to four panes is disabled while 2D mode is active and can be restored immediately after returning to 3D.

## Production guidance

For competitive games, keep gameplay simulation independent of the editor camera profile. Use the 2D profile for authoring collision shapes, spawn layouts, hitboxes, navigation markers, and Blueprint/RbScript gameplay entities; validate those resources through the same deterministic gameplay and network tests used by 3D projects. The profile is an editor contract, not a simulation shortcut: fixed-step simulation, rollback, temporal replay, and digest validation remain unchanged.

## Known scope

The profile provides a production-oriented XY authoring workflow and parity of the shared engine services. It does not claim automatic sprite import, tilemap authoring, 2D skeletal animation, or a specialized 2D renderer in this change. Those systems should be added as explicit, testable extensions rather than implied by the viewport mode.
