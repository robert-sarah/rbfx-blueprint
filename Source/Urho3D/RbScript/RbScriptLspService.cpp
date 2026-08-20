// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#include "RbScriptLspService.h"

#include "RbScriptLexer.h"
#include "RbScriptParser.h"

#include <algorithm>
#include <cctype>

namespace Urho3D
{

namespace
{

const char* Keywords[] =
{
    "module", "use", "script", "fn", "return", "if", "else", "while", "for", "in", "let", "var", "const",
    "struct", "enum", "class", "signal", "on", "async", "await", "emit", "match", "break", "continue", "public", "private", "static"
};

bool PositionBeforeOrEqual(const RbScriptLspPosition& lhs, const RbScriptLspPosition& rhs)
{
    return lhs.line < rhs.line || (lhs.line == rhs.line && lhs.character <= rhs.character);
}

unsigned DiagnosticSeverityValue(RbScriptDiagnosticSeverity severity)
{
    switch (severity)
    {
    case RbScriptDiagnosticSeverity::Error: return 1;
    case RbScriptDiagnosticSeverity::Warning: return 2;
    case RbScriptDiagnosticSeverity::Info: return 3;
    default: return 1;
    }
}

unsigned CompletionKindValue(RbScriptLspCompletionKind kind)
{
    switch (kind)
    {
    case RbScriptLspCompletionKind::Keyword: return 14;
    case RbScriptLspCompletionKind::Type: return 7;
    case RbScriptLspCompletionKind::Function: return 3;
    case RbScriptLspCompletionKind::Symbol: return 6;
    default: return 6;
    }
}

} // namespace

RbScriptLspService::RbScriptLspService(Context* context)
    : context_(context)
{
}

void RbScriptLspService::SetError(ea::string* error, const ea::string& message) const
{
    lastError_ = message;
    if (error)
        *error = message;
}

bool RbScriptLspService::OpenDocument(const ea::string& uri, const ea::string& source, unsigned version, ea::string* error)
{
    if (uri.empty() || version == 0)
    {
        SetError(error, "An rbscript document requires a non-empty URI and a positive version.");
        return false;
    }
    if (documents_.find(uri) != documents_.end())
    {
        SetError(error, Format("Document '{}' is already open.", uri));
        return false;
    }
    RbScriptLspDocument document;
    document.uri = uri;
    document.version = version;
    document.source = source;
    if (!IndexDocument(document, error))
        return false;
    documents_[uri] = ea::move(document);
    return true;
}

bool RbScriptLspService::UpdateDocument(const ea::string& uri, const ea::string& source, unsigned version, ea::string* error)
{
    auto it = documents_.find(uri);
    if (it == documents_.end())
    {
        SetError(error, Format("Document '{}' is not open.", uri));
        return false;
    }
    if (version <= it->second.version)
    {
        SetError(error, "Document versions must increase monotonically.");
        return false;
    }
    RbScriptLspDocument candidate = it->second;
    candidate.version = version;
    candidate.source = source;
    if (!IndexDocument(candidate, error))
        return false;
    it->second = ea::move(candidate);
    return true;
}

bool RbScriptLspService::CloseDocument(const ea::string& uri, ea::string* error)
{
    auto it = documents_.find(uri);
    if (it == documents_.end())
    {
        SetError(error, Format("Document '{}' is not open.", uri));
        return false;
    }
    documents_.erase(it);
    return true;
}

bool RbScriptLspService::Revalidate(const ea::string& uri, ea::string* error)
{
    auto it = documents_.find(uri);
    if (it == documents_.end())
    {
        SetError(error, Format("Document '{}' is not open.", uri));
        return false;
    }
    return IndexDocument(it->second, error);
}

const RbScriptLspDocument* RbScriptLspService::GetDocument(const ea::string& uri) const
{
    const auto it = documents_.find(uri);
    return it == documents_.end() ? nullptr : &it->second;
}

const ea::vector<RbScriptDiagnostic>* RbScriptLspService::GetDiagnostics(const ea::string& uri) const
{
    const RbScriptLspDocument* document = GetDocument(uri);
    return document ? &document->diagnostics : nullptr;
}

bool RbScriptLspService::IndexDocument(RbScriptLspDocument& document, ea::string* error) const
{
    RbScriptLexer lexer(document.source, document.uri);
    document.tokens = lexer.Tokenize();
    document.diagnostics = lexer.GetDiagnostics();
    document.symbols.clear();

    RbScriptParser parser(document.tokens, document.uri);
    RbScriptModule module = parser.ParseModule();
    for (const RbScriptDiagnostic& diagnostic : parser.GetDiagnostics())
        document.diagnostics.push_back(diagnostic);

    if (registry_ && !module.diagnostics.empty())
    {
        RbScriptTypeChecker checker(*registry_);
        checker.Check(module);
        for (const RbScriptDiagnostic& diagnostic : checker.GetDiagnostics())
            document.diagnostics.push_back(diagnostic);
    }

    for (unsigned i = 0; i + 1 < document.tokens.size(); ++i)
    {
        const RbScriptToken& keyword = document.tokens[i];
        const RbScriptToken& name = document.tokens[i + 1];
        if (name.kind != RbScriptTokenKind::Identifier)
            continue;

        const char* kind = nullptr;
        const char* detail = nullptr;
        switch (keyword.kind)
        {
        case RbScriptTokenKind::Module: kind = "module"; detail = "module"; break;
        case RbScriptTokenKind::Script: kind = "script"; detail = "script"; break;
        case RbScriptTokenKind::Fn: kind = "function"; detail = "function"; break;
        case RbScriptTokenKind::Var:
        case RbScriptTokenKind::Let:
        case RbScriptTokenKind::Const: kind = "field"; detail = "field"; break;
        case RbScriptTokenKind::Struct: kind = "struct"; detail = "struct"; break;
        case RbScriptTokenKind::Enum: kind = "enum"; detail = "enum"; break;
        case RbScriptTokenKind::Class: kind = "class"; detail = "class"; break;
        default: break;
        }
        if (!kind)
            continue;
        bool duplicate = false;
        for (const RbScriptLspSymbol& symbol : document.symbols)
        {
            if (symbol.name == name.lexeme && symbol.kind == kind)
            {
                duplicate = true;
                break;
            }
        }
        if (!duplicate)
            document.symbols.push_back({name.lexeme, kind, detail, ToRange(name.span)});
    }
    (void)context_;
    return true;
}

RbScriptLspPosition RbScriptLspService::ToPosition(const RbScriptSourcePosition& position)
{
    return {position.line > 0 ? position.line - 1 : 0, position.column > 0 ? position.column - 1 : 0};
}

RbScriptLspRange RbScriptLspService::ToRange(const RbScriptSourceSpan& span)
{
    return {ToPosition(span.begin), ToPosition(span.end)};
}

const RbScriptToken* RbScriptLspService::FindTokenAt(const RbScriptLspDocument& document, const RbScriptLspPosition& position) const
{
    for (const RbScriptToken& token : document.tokens)
    {
        const RbScriptLspRange range = ToRange(token.span);
        if (PositionBeforeOrEqual(range.start, position) && PositionBeforeOrEqual(position, range.end))
            return &token;
    }
    return nullptr;
}

const RbScriptLspSymbol* RbScriptLspService::FindSymbol(const RbScriptLspDocument& document, const ea::string& name) const
{
    for (const RbScriptLspSymbol& symbol : document.symbols)
    {
        if (symbol.name == name)
            return &symbol;
    }
    return nullptr;
}

bool RbScriptLspService::IsIdentifier(const ea::string& value)
{
    if (value.empty() || (!std::isalpha(static_cast<unsigned char>(value[0])) && value[0] != '_'))
        return false;
    return std::all_of(value.begin() + 1, value.end(), [](char c)
    {
        return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
    });
}

ea::vector<RbScriptLspCompletionItem> RbScriptLspService::Complete(const ea::string& uri, const RbScriptLspPosition& position) const
{
    ea::vector<RbScriptLspCompletionItem> result;
    const RbScriptLspDocument* document = GetDocument(uri);
    if (!document)
        return result;

    ea::string prefix;
    if (const RbScriptToken* token = FindTokenAt(*document, position))
    {
        if (token->kind == RbScriptTokenKind::Identifier)
            prefix = token->lexeme;
    }
    ea::unordered_map<ea::string, bool> seen;
    auto add = [&](const ea::string& label, const ea::string& detail, RbScriptLspCompletionKind kind)
    {
        if (!prefix.empty() && !label.starts_with(prefix) || label.empty() || seen.find(label) != seen.end())
            return;
        seen[label] = true;
        result.push_back({label, detail, kind});
    };
    for (const char* keyword : Keywords)
        add(keyword, "rbscript keyword", RbScriptLspCompletionKind::Keyword);
    if (registry_)
    {
        for (const ea::string& type : registry_->GetTypeNames())
            add(type, "rbscript type", RbScriptLspCompletionKind::Type);
        for (const ea::string& function : registry_->GetFunctionNames())
        {
            const RbScriptFunctionSignature* signature = registry_->FindFunction(function);
            add(function, signature ? Format("fn -> {}", signature->returnType.ToString()) : "rbscript function",
                RbScriptLspCompletionKind::Function);
        }
    }
    for (const RbScriptLspSymbol& symbol : document->symbols)
        add(symbol.name, symbol.detail, RbScriptLspCompletionKind::Symbol);
    std::sort(result.begin(), result.end(), [](const auto& lhs, const auto& rhs) { return lhs.label < rhs.label; });
    if (result.size() > 200)
        result.resize(200);
    return result;
}

bool RbScriptLspService::GoToDefinition(const ea::string& uri, const RbScriptLspPosition& position, RbScriptLspLocation& location) const
{
    const RbScriptLspDocument* document = GetDocument(uri);
    if (!document)
        return false;
    const RbScriptToken* token = FindTokenAt(*document, position);
    if (!token || token->kind != RbScriptTokenKind::Identifier)
        return false;

    if (const RbScriptLspSymbol* symbol = FindSymbol(*document, token->lexeme))
    {
        location.uri = uri;
        location.range = symbol->range;
        return true;
    }

    for (const auto& entry : documents_)
    {
        for (const RbScriptLspSymbol& symbol : entry.second.symbols)
        {
            if (symbol.name == token->lexeme)
            {
                location.uri = entry.first;
                location.range = symbol.range;
                return true;
            }
        }
    }
    return false;
}

ea::vector<RbScriptLspLocation> RbScriptLspService::FindReferences(const ea::string& uri,
    const RbScriptLspPosition& position, bool includeDeclaration) const
{
    ea::vector<RbScriptLspLocation> result;
    const RbScriptLspDocument* document = GetDocument(uri);
    if (!document)
        return result;
    const RbScriptToken* target = FindTokenAt(*document, position);
    if (!target || target->kind != RbScriptTokenKind::Identifier)
        return result;

    bool declared = false;
    for (const auto& entry : documents_)
    {
        for (const RbScriptLspSymbol& symbol : entry.second.symbols)
        {
            if (symbol.name == target->lexeme)
            {
                declared = true;
                break;
            }
        }
        if (declared)
            break;
    }
    if (!declared)
        return result;

    for (const auto& entry : documents_)
    {
        const RbScriptLspDocument& candidate = entry.second;
        for (const RbScriptToken& token : candidate.tokens)
        {
            if (token.kind != RbScriptTokenKind::Identifier || token.lexeme != target->lexeme)
                continue;

            bool isDeclaration = false;
            for (const RbScriptLspSymbol& symbol : candidate.symbols)
            {
                const RbScriptLspRange range = ToRange(token.span);
                if (symbol.name == token.lexeme && symbol.range.start.line == range.start.line
                    && symbol.range.start.character == range.start.character)
                {
                    isDeclaration = true;
                    break;
                }
            }
            if (!includeDeclaration && isDeclaration)
                continue;
            result.push_back({entry.first, ToRange(token.span)});
        }
    }

    std::sort(result.begin(), result.end(), [](const RbScriptLspLocation& lhs, const RbScriptLspLocation& rhs)
    {
        if (lhs.uri != rhs.uri)
            return lhs.uri < rhs.uri;
        if (lhs.range.start.line != rhs.range.start.line)
            return lhs.range.start.line < rhs.range.start.line;
        return lhs.range.start.character < rhs.range.start.character;
    });
    return result;
}

ea::vector<RbScriptLspWorkspaceSymbol> RbScriptLspService::WorkspaceSymbols(const ea::string& query) const
{
    ea::vector<RbScriptLspWorkspaceSymbol> result;
    for (const auto& entry : documents_)
    {
        for (const RbScriptLspSymbol& symbol : entry.second.symbols)
        {
            if (!query.empty() && symbol.name.find(query) == ea::string::npos)
                continue;
            result.push_back({symbol.name, symbol.kind, symbol.detail, {entry.first, symbol.range}});
        }
    }

    std::sort(result.begin(), result.end(), [](const RbScriptLspWorkspaceSymbol& lhs, const RbScriptLspWorkspaceSymbol& rhs)
    {
        if (lhs.name != rhs.name)
            return lhs.name < rhs.name;
        if (lhs.location.uri != rhs.location.uri)
            return lhs.location.uri < rhs.location.uri;
        if (lhs.location.range.start.line != rhs.location.range.start.line)
            return lhs.location.range.start.line < rhs.location.range.start.line;
        return lhs.location.range.start.character < rhs.location.range.start.character;
    });
    return result;
}

ea::vector<RbScriptLspTextEdit> RbScriptLspService::Rename(const ea::string& uri, const RbScriptLspPosition& position,
    const ea::string& newName, ea::string* error) const
{
    ea::vector<RbScriptLspTextEdit> result;
    if (!IsIdentifier(newName))
    {
        SetError(error, "The new rbscript symbol name is not a valid identifier.");
        return result;
    }
    const RbScriptLspDocument* document = GetDocument(uri);
    if (!document)
    {
        SetError(error, Format("Document '{}' is not open.", uri));
        return result;
    }
    const RbScriptToken* target = FindTokenAt(*document, position);
    if (!target || target->kind != RbScriptTokenKind::Identifier || !FindSymbol(*document, target->lexeme))
    {
        SetError(error, "Rename requires a declared rbscript symbol.");
        return result;
    }
    for (const RbScriptToken& token : document->tokens)
    {
        if (token.kind == RbScriptTokenKind::Identifier && token.lexeme == target->lexeme)
            result.push_back({ToRange(token.span), newName});
    }
    return result;
}

ea::string RbScriptLspService::Hover(const ea::string& uri, const RbScriptLspPosition& position) const
{
    const RbScriptLspDocument* document = GetDocument(uri);
    if (!document)
        return {};
    const RbScriptToken* token = FindTokenAt(*document, position);
    if (!token)
        return {};
    if (const RbScriptLspSymbol* symbol = FindSymbol(*document, token->lexeme))
        return Format("{} `{}`\n\nDeclared in {}.", symbol->kind, symbol->name, symbol->detail);
    if (registry_)
    {
        if (const RbScriptFunctionSignature* function = registry_->FindFunction(token->lexeme))
            return Format("fn {} -> {}", function->name, function->returnType.ToString());
        const RbScriptType type = registry_->Resolve(token->lexeme);
        if (type.IsValid())
            return Format("type {}", type.ToString());
    }
    return {};
}

JSONValue RbScriptLspService::ToJson(const RbScriptLspRange& range)
{
    JSONValue result(JSON_OBJECT);
    JSONValue start(JSON_OBJECT);
    start.Set("line", range.start.line);
    start.Set("character", range.start.character);
    JSONValue end(JSON_OBJECT);
    end.Set("line", range.end.line);
    end.Set("character", range.end.character);
    result.Set("start", ea::move(start));
    result.Set("end", ea::move(end));
    return result;
}

JSONValue RbScriptLspService::ToJson(const RbScriptLspLocation& location)
{
    JSONValue result(JSON_OBJECT);
    result.Set("uri", location.uri);
    result.Set("range", ToJson(location.range));
    return result;
}

JSONValue RbScriptLspService::ToJson(const RbScriptLspWorkspaceSymbol& symbol)
{
    JSONValue result(JSON_OBJECT);
    result.Set("name", symbol.name);
    result.Set("kind", symbol.kind);
    result.Set("detail", symbol.detail);
    result.Set("location", ToJson(symbol.location));
    return result;
}

JSONValue RbScriptLspService::ToJson(const RbScriptLspCompletionItem& item)
{
    JSONValue result(JSON_OBJECT);
    result.Set("label", item.label);
    result.Set("detail", item.detail);
    result.Set("kind", CompletionKindValue(item.kind));
    return result;
}

JSONValue RbScriptLspService::ToJson(const RbScriptLspTextEdit& edit)
{
    JSONValue result(JSON_OBJECT);
    result.Set("range", ToJson(edit.range));
    result.Set("newText", edit.newText);
    return result;
}

JSONValue RbScriptLspService::ToJson(const RbScriptDiagnostic& diagnostic)
{
    JSONValue result(JSON_OBJECT);
    result.Set("range", ToJson(ToRange(diagnostic.span)));
    result.Set("severity", DiagnosticSeverityValue(diagnostic.severity));
    result.Set("code", diagnostic.code);
    result.Set("message", diagnostic.message);
    return result;
}

RbScriptLspPosition RbScriptLspService::JsonPosition(const JSONValue& params)
{
    const JSONValue& position = params.Get("position");
    return {position.Get("line").GetUInt(), position.Get("character").GetUInt()};
}

ea::string RbScriptLspService::JsonUri(const JSONValue& params)
{
    const JSONValue& textDocument = params.Get("textDocument");
    return textDocument.Get("uri").GetString();
}

ea::string RbScriptLspService::JsonMethod(const JSONValue& request)
{
    return request.Get("method").GetString();
}

bool RbScriptLspService::HandleJsonRpc(const JSONValue& request, JSONValue& response, ea::string* error)
{
    if (!request.IsObject() || !request.Get("method").IsString())
    {
        SetError(error, "LSP request requires an object and a method string.");
        return false;
    }
    response = JSONValue(JSON_OBJECT);
    response.Set("jsonrpc", "2.0");
    if (request.Get("id").IsNumber() || request.Get("id").IsString())
        response.Set("id", request.Get("id"));
    const ea::string method = JsonMethod(request);
    const JSONValue& params = request.Get("params");

    if (method == "initialize")
    {
        JSONValue result(JSON_OBJECT);
        result.Set("serverInfo", "rbfx-blueprint rbscript");
        JSONValue capabilities(JSON_OBJECT);
        capabilities.Set("textDocumentSync", 1u);
        capabilities.Set("completionProvider", JSONValue(JSON_OBJECT));
        capabilities.Set("definitionProvider", true);
        capabilities.Set("hoverProvider", true);
        capabilities.Set("renameProvider", true);
        capabilities.Set("referencesProvider", true);
        capabilities.Set("workspaceSymbolProvider", true);
        result.Set("capabilities", ea::move(capabilities));
        response.Set("result", ea::move(result));
        return true;
    }
    if (method == "shutdown")
    {
        response.Set("result", JSONValue(JSON_NULL));
        return true;
    }
    ea::string operationError;
    if (method == "textDocument/didOpen")
    {
        const JSONValue& textDocument = params.Get("textDocument");
        const bool ok = OpenDocument(textDocument.Get("uri").GetString(), textDocument.Get("text").GetString(),
            textDocument.Get("version").GetUInt(), &operationError);
        if (!ok)
            goto request_error;
        response.Set("result", JSONValue(JSON_NULL));
        return true;
    }
    if (method == "textDocument/didChange")
    {
        const JSONValue& textDocument = params.Get("textDocument");
        const JSONValue& changes = params.Get("contentChanges");
        if (!changes.IsArray() || changes.GetArray().empty())
        {
            operationError = "didChange requires at least one content change.";
            goto request_error;
        }
        const bool ok = UpdateDocument(textDocument.Get("uri").GetString(), changes.GetArray().front().Get("text").GetString(),
            textDocument.Get("version").GetUInt(), &operationError);
        if (!ok)
            goto request_error;
        response.Set("result", JSONValue(JSON_NULL));
        return true;
    }
    if (method == "textDocument/completion")
    {
        JSONValue items(JSON_ARRAY);
        for (const auto& item : Complete(JsonUri(params), JsonPosition(params)))
            items.Push(ToJson(item));
        response.Set("result", ea::move(items));
        return true;
    }
    if (method == "textDocument/definition")
    {
        RbScriptLspLocation location;
        if (!GoToDefinition(JsonUri(params), JsonPosition(params), location))
        {
            response.Set("result", JSONValue(JSON_NULL));
            return true;
        }
        response.Set("result", ToJson(location));
        return true;
    }
    if (method == "textDocument/references")
    {
        bool includeDeclaration = true;
        const JSONValue& context = params.Get("context");
        if (context.IsObject() && context.Contains("includeDeclaration"))
            includeDeclaration = context.Get("includeDeclaration").GetBool(true);
        JSONValue references(JSON_ARRAY);
        for (const RbScriptLspLocation& reference : FindReferences(JsonUri(params), JsonPosition(params), includeDeclaration))
            references.Push(ToJson(reference));
        response.Set("result", ea::move(references));
        return true;
    }
    if (method == "workspace/symbol")
    {
        JSONValue symbols(JSON_ARRAY);
        const ea::string query = params.IsObject() ? params.Get("query").GetString() : ea::string();
        for (const RbScriptLspWorkspaceSymbol& symbol : WorkspaceSymbols(query))
            symbols.Push(ToJson(symbol));
        response.Set("result", ea::move(symbols));
        return true;
    }
    if (method == "textDocument/hover")
    {
        const ea::string contents = Hover(JsonUri(params), JsonPosition(params));
        if (contents.empty())
        {
            response.Set("result", JSONValue(JSON_NULL));
            return true;
        }
        JSONValue result(JSON_OBJECT);
        result.Set("contents", contents);
        response.Set("result", ea::move(result));
        return true;
    }
    if (method == "textDocument/rename")
    {
        const ea::vector<RbScriptLspTextEdit> edits = Rename(JsonUri(params), JsonPosition(params), params.Get("newName").GetString(), &operationError);
        if (edits.empty() && !operationError.empty())
            goto request_error;
        JSONValue documentEdits(JSON_ARRAY);
        for (const auto& edit : edits)
            documentEdits.Push(ToJson(edit));
        JSONValue changes(JSON_OBJECT);
        changes.Set(JsonUri(params), ea::move(documentEdits));
        JSONValue result(JSON_OBJECT);
        result.Set("changes", ea::move(changes));
        response.Set("result", ea::move(result));
        return true;
    }
    operationError = Format("Unsupported LSP method '{}'.", method);

request_error:
    response.Erase("result");
    JSONValue lspError(JSON_OBJECT);
    lspError.Set("code", -32602);
    lspError.Set("message", operationError);
    response.Set("error", ea::move(lspError));
    SetError(error, operationError);
    return false;
}

} // namespace Urho3D
