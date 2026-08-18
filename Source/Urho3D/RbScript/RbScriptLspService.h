// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include "RbScriptDefs.h"
#include "RbScriptType.h"

#include <Urho3D/Resource/JSONValue.h>

#include <EASTL/vector.h>
#include <EASTL/unordered_map.h>

namespace Urho3D
{

class Context;
class RbScriptLexer;

struct URHO3D_API RbScriptLspPosition
{
    unsigned line{};
    unsigned character{};
};

struct URHO3D_API RbScriptLspRange
{
    RbScriptLspPosition start;
    RbScriptLspPosition end;
};

struct URHO3D_API RbScriptLspLocation
{
    ea::string uri;
    RbScriptLspRange range;
};

enum class RbScriptLspCompletionKind
{
    Keyword,
    Type,
    Function,
    Symbol,
};

struct URHO3D_API RbScriptLspCompletionItem
{
    ea::string label;
    ea::string detail;
    RbScriptLspCompletionKind kind{RbScriptLspCompletionKind::Symbol};
};

struct URHO3D_API RbScriptLspTextEdit
{
    RbScriptLspRange range;
    ea::string newText;
};

struct URHO3D_API RbScriptLspSymbol
{
    ea::string name;
    ea::string kind;
    ea::string detail;
    RbScriptLspRange range;
};

struct URHO3D_API RbScriptLspDocument
{
    ea::string uri;
    unsigned version{};
    ea::string source;
    ea::vector<RbScriptToken> tokens;
    ea::vector<RbScriptDiagnostic> diagnostics;
    ea::vector<RbScriptLspSymbol> symbols;
};

/// Transport-independent Language Server Protocol service for rbscript.
/// The editor or a standalone stdio/WebSocket adapter can feed JSON-RPC messages
/// into HandleJsonRpc without duplicating parsing, reflection or source indexing.
class URHO3D_API RbScriptLspService
{
public:
    explicit RbScriptLspService(Context* context = nullptr);

    void SetTypeRegistry(const RbScriptTypeRegistry* registry) { registry_ = registry; }
    const RbScriptTypeRegistry* GetTypeRegistry() const { return registry_; }

    bool OpenDocument(const ea::string& uri, const ea::string& source, unsigned version = 1, ea::string* error = nullptr);
    bool UpdateDocument(const ea::string& uri, const ea::string& source, unsigned version, ea::string* error = nullptr);
    bool CloseDocument(const ea::string& uri, ea::string* error = nullptr);
    bool Revalidate(const ea::string& uri, ea::string* error = nullptr);

    const RbScriptLspDocument* GetDocument(const ea::string& uri) const;
    const ea::vector<RbScriptDiagnostic>* GetDiagnostics(const ea::string& uri) const;
    ea::vector<RbScriptLspCompletionItem> Complete(const ea::string& uri, const RbScriptLspPosition& position) const;
    bool GoToDefinition(const ea::string& uri, const RbScriptLspPosition& position, RbScriptLspLocation& location) const;
    ea::vector<RbScriptLspTextEdit> Rename(const ea::string& uri, const RbScriptLspPosition& position, const ea::string& newName,
        ea::string* error = nullptr) const;
    ea::string Hover(const ea::string& uri, const RbScriptLspPosition& position) const;

    /// Handle one JSON-RPC-like request object and produce a JSON-RPC-like response.
    /// Supported methods: initialize, textDocument/didOpen, textDocument/didChange,
    /// textDocument/completion, textDocument/definition, textDocument/rename,
    /// textDocument/hover and shutdown.
    bool HandleJsonRpc(const JSONValue& request, JSONValue& response, ea::string* error = nullptr);

    const ea::unordered_map<ea::string, RbScriptLspDocument>& GetDocuments() const { return documents_; }
    const ea::string& GetLastError() const { return lastError_; }

private:
    void SetError(ea::string* error, const ea::string& message) const;
    bool IndexDocument(RbScriptLspDocument& document, ea::string* error) const;
    const RbScriptToken* FindTokenAt(const RbScriptLspDocument& document, const RbScriptLspPosition& position) const;
    const RbScriptLspSymbol* FindSymbol(const RbScriptLspDocument& document, const ea::string& name) const;
    static RbScriptLspRange ToRange(const RbScriptSourceSpan& span);
    static RbScriptLspPosition ToPosition(const RbScriptSourcePosition& position);
    static bool IsIdentifier(const ea::string& value);
    static ea::string JsonMethod(const JSONValue& request);
    static ea::string JsonUri(const JSONValue& params);
    static RbScriptLspPosition JsonPosition(const JSONValue& params);
    static JSONValue ToJson(const RbScriptLspRange& range);
    static JSONValue ToJson(const RbScriptLspLocation& location);
    static JSONValue ToJson(const RbScriptLspCompletionItem& item);
    static JSONValue ToJson(const RbScriptLspTextEdit& edit);
    static JSONValue ToJson(const RbScriptDiagnostic& diagnostic);

    Context* context_{};
    const RbScriptTypeRegistry* registry_{};
    mutable ea::unordered_map<ea::string, RbScriptLspDocument> documents_;
    mutable ea::string lastError_;
};

} // namespace Urho3D
