# Deterministic Terrain Authoring

## Purpose

`TerrainAuthoring` is the deterministic core used by the World Fabric terrain panel. It is intentionally separated from the renderer and from editor state so terrain brush operations can be tested, replayed, hashed and later connected to a real terrain resource or streaming backend.

The current contract operates on a bounded floating-point heightfield. Brush operations are quantized before they are written, which makes the resulting heightfield stable across repeated runs using the same settings and input coordinates.

## Brush settings

| Setting | Meaning |
| --- | --- |
| `radius` | Brush radius in heightfield cells. It must be positive and finite. |
| `strength` | Signed height delta before quantization. |
| `quantization` | Positive number of quantization steps per world unit. |
| `falloff` | `Constant`, `Linear`, or `SmoothStep`. |

`ValidateTerrainBrushSettings` rejects non-finite values, a non-positive radius, a non-positive quantization factor, and unsupported falloff values. Invalid settings do not modify the heightfield.

## Deterministic stamping

`StampTerrainHeight` evaluates the brush influence for every cell in the bounded heightfield. The influence is calculated from the squared distance to the brush center and the selected falloff. The resulting delta is quantized using a symmetric rounding rule before it is added to the cell.

The operation is deterministic for a fixed heightfield size, brush settings, center, and initial heightfield. The function returns a boolean rather than silently clamping invalid input, allowing an editor command or a gameplay tool to reject an invalid operation before recording it in an undo stack or a replay.

## World Fabric integration

`WorldFabricTab` owns the preview heightfield and the editor-facing brush state. Its panel exposes the brush radius, strength, falloff, center coordinates and an **Apply Stamp** command. The preview is rendered as a compact table of height values and displays the stable terrain digest used for inspection and future reproduction workflows.

The panel is an authoring surface, not a complete terrain renderer. It does not currently bake GPU clipmaps, generate vegetation, create collision meshes, or stream terrain tiles. Those systems should consume the deterministic heightfield contract rather than duplicate brush logic.

## Testing contract

The Catch2 coverage in `TestTerrainAuthoring.cpp` verifies the following production behaviors:

| Behavior | Covered result |
| --- | --- |
| Falloff ordering | Constant influence is not below linear influence at the brush center, and smooth-step influence is bounded. |
| Quantized stamping | A valid stamp changes the heightfield in quantized increments. |
| Digest stability | Equal heightfields produce equal digests, while a changed cell changes the digest. |
| Invalid settings | Invalid radius and quantization values are rejected without an undefined operation. |

## Extension path

The next compatible extensions are a tile-aware heightfield resource, dirty-region reporting for incremental rebuilds, collision and navigation baking requests, and World Fabric dependency edges from terrain tiles to generated vegetation, water and navigation artifacts.
