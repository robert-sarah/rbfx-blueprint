# RbScript Language Server

`RbScriptLspService` is the transport-independent language service for `.rbscript` documents. The editor, a standalone stdio adapter, or a WebSocket adapter can feed JSON-RPC objects into the same service without duplicating lexing, parsing, reflection, or symbol indexing.

## Workspace model

The service keeps an indexed document table keyed by URI. Each opened document stores its version, source text, tokens, diagnostics, and declared symbols. `OpenDocument`, `UpdateDocument`, `CloseDocument`, and `Revalidate` are transactional at document level: a failed re-index does not replace the last valid indexed document.

Workspace queries are deterministic. Results are sorted by symbol name, URI, line, and character, so editor panels and CI diagnostics do not depend on the iteration order of an EASTL hash table.

| API | Purpose |
|---|---|
| `OpenDocument` / `UpdateDocument` | Add or replace a versioned in-memory document. |
| `GoToDefinition` | Resolve a local declaration first, then search declarations in all open documents. |
| `FindReferences` | Return identifier references across all open documents, optionally excluding declarations. |
| `WorkspaceSymbols` | Search indexed declarations by name and return URI/range locations. |
| `Complete` / `Hover` | Combine document symbols with the reflected rbfx type and function registry. |
| `Rename` | Produce source edits for every matching identifier in the selected document. |

The current cross-file resolver intentionally uses the declared symbol name as the workspace key. This gives predictable tooling for the present rbscript module model while leaving room for a future module-qualified symbol identity.

## JSON-RPC capabilities

`initialize` advertises completion, definition, hover, rename, references, and workspace-symbol support. The service currently handles the following document and workspace methods:

```text
initialize
shutdown
textDocument/didOpen
textDocument/didChange
textDocument/completion
textDocument/definition
textDocument/references
textDocument/hover
textDocument/rename
workspace/symbol
```

A references request can exclude declarations with the standard context flag:

```json
{
  "jsonrpc": "2.0",
  "id": 12,
  "method": "textDocument/references",
  "params": {
    "textDocument": { "uri": "file:///project/Player.rbscript" },
    "position": { "line": 8, "character": 19 },
    "context": { "includeDeclaration": false }
  }
}
```

A workspace symbol request returns LSP-style symbols with a name, kind, detail, and a location containing the source URI and range:

```json
{
  "jsonrpc": "2.0",
  "id": 13,
  "method": "workspace/symbol",
  "params": { "query": "apply_damage" }
}
```

## Editor integration

The Foundation `RbScriptTab` can use the service directly for local completion, definition, hover, and rename. A project-level adapter can open every `.rbscript` resource as it enters the workspace, forward file changes with monotonically increasing versions, and use `FindReferences` or `WorkspaceSymbols` for project search panels. No network transport or external language server process is required.

The implementation is deliberately transport-neutral. This keeps the same symbol ranges and diagnostics in the local editor, a CI validation command, and a future external IDE adapter.

## Example workflow

The repository contains [`Examples/RbScript/CompetitivePlayer.rbscript`](../Examples/RbScript/CompetitivePlayer.rbscript), a small deterministic gameplay controller. Open it together with another script that calls `record_input`; `GoToDefinition` resolves the declaration across URIs and `FindReferences` reports both the declaration and the call site.

The validation coverage is in `Source/Tests/TestPublicExtensionInfrastructure.cpp`. It proves cross-document definition lookup, deterministic reference ordering, declaration filtering, workspace symbol search, and the JSON-RPC capability and request shapes.

## Scope and next extensions

The service is production-oriented for an embedded single-process workspace index. It does not yet persist an index to disk, watch the filesystem, or implement semantic module imports. Those concerns belong in the project/editor adapter and can be added without changing the transport-independent document contract.
