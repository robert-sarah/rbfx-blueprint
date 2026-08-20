# Shader Graph Production Contract

## Scope

The `ShaderGraph` resource provides a backend-neutral material graph with deterministic GLSL and HLSL source generation. The graph is intentionally small and explicit: constants, parameters, arithmetic, interpolation, texture sampling and one surface output are represented as typed nodes.

The production contract is to fail early on malformed graph data instead of emitting source that appears valid in the editor but fails later in a shader compiler.

## Validation rules

`ShaderGraph::Validate` enforces the following invariants:

| Rule | Failure condition |
| --- | --- |
| One output | The graph does not contain exactly one live `Output` node, or the selected output is not that node. |
| Unique parameters | A parameter has an empty or duplicated name. |
| Resolved references | A `Parameter` or `TextureSample` node refers to a parameter that is not declared in the graph. |
| Texture typing | A `TextureSample` node refers to a parameter whose declared type is not `Texture2D`. |
| Connection integrity | A connection references a missing node, an empty pin, or a second connection to the same input pin. |
| Evaluability | The output expression is missing or contains a cycle. |

Validation errors are returned through the existing `ea::string` error argument, allowing the editor, resource loader and CI tests to show the same diagnostic.

## Code generation

GLSL generation emits a fragment shader with `sampler2D` uniforms and a `fragColor` output. HLSL generation emits a pixel shader entry point with `Texture2D` resources and a matching `SamplerState` for every texture parameter. The sampler declaration is generated from the same sanitized parameter name as the texture expression, preventing a common class of source-generation errors.

Parameter names are sanitized before they become shader identifiers. A name that is empty after sanitization or begins with a digit receives a `p_` prefix. The logical parameter name remains unchanged in the serialized graph.

## Editor workflow

`ShaderGraphTab` exposes validation and separate GLSL/HLSL generation commands. Validation is run after demo reset and can be invoked before generation. The editor stores the generated source and the diagnostic string in the tab state so a project can connect them to a source preview, compiler invocation or material preview without changing the graph model.

## Tests and evidence

`TestShaderVfxAudio.cpp` covers valid GLSL/HLSL generation, resource JSON round-trips, duplicate input rejection, missing parameter rejection, duplicate output rejection and HLSL sampler-state emission. The source units compile with the project’s C++17 include configuration. A complete runtime test must use one coherent CMake/Ninja build directory; mixing newly compiled objects with an older shared engine library is not a valid ABI test.

## Extension path

The next compatible extensions are explicit pin schemas, type conversion nodes, sampler-state settings, material domain/output profiles, shader compiler diagnostics, include dependency tracking and cache keys linked to the reproducible asset manifest. These should preserve the current rule that invalid graphs are rejected before code generation.
