// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include "RbScriptType.h"

#include <Urho3D/Resource/JSONValue.h>

#include <EASTL/vector.h>

namespace Urho3D
{

struct URHO3D_API DocumentationPage
{
    ea::string id;
    ea::string title;
    ea::string category;
    ea::string markdown;
    ea::vector<ea::string> tags;
};

struct URHO3D_API DocumentationSymbol
{
    ea::string name;
    ea::string qualifiedName;
    ea::string kind;
    ea::string signature;
    ea::string description;
    ea::string sourceUri;
    ea::string since;
};

struct URHO3D_API DocumentationSearchResult
{
    ea::string id;
    ea::string title;
    ea::string kind;
    ea::string snippet;
    unsigned score{};
};

/// Searchable, deterministic documentation index shared by the editor, LSP hover
/// and future web/desktop documentation viewers.
class URHO3D_API InteractiveDocumentation
{
public:
    bool AddPage(const DocumentationPage& page, ea::string* error = nullptr);
    bool AddSymbol(const DocumentationSymbol& symbol, ea::string* error = nullptr);
    bool RemovePage(const ea::string& id, ea::string* error = nullptr);
    bool RemoveSymbol(const ea::string& qualifiedName, ea::string* error = nullptr);
    void Clear();

    void IndexTypeRegistry(const RbScriptTypeRegistry& registry, const ea::string& sourceUri = "reflection://rbscript");
    ea::vector<DocumentationSearchResult> Search(const ea::string& query, unsigned limit = 50) const;
    const DocumentationPage* FindPage(const ea::string& id) const;
    const DocumentationSymbol* FindSymbol(const ea::string& qualifiedName) const;
    ea::string RenderMarkdown(const ea::string& id) const;
    ea::string RenderHtml(const ea::string& id) const;
    JSONValue ToJSON() const;
    bool FromJSON(const JSONValue& root, ea::string* error = nullptr);
    bool Validate(ea::string* error = nullptr) const;
    unsigned long long ComputeDigest(ea::string* error = nullptr) const;

    const ea::vector<DocumentationPage>& GetPages() const { return pages_; }
    const ea::vector<DocumentationSymbol>& GetSymbols() const { return symbols_; }
    const ea::string& GetLastError() const { return lastError_; }

private:
    void SetError(ea::string* error, const ea::string& message) const;
    static ea::string Lowercase(const ea::string& value);
    static ea::string EscapeHtml(const ea::string& value);
    static unsigned Score(const ea::string& haystack, const ea::string& query);

    ea::vector<DocumentationPage> pages_;
    ea::vector<DocumentationSymbol> symbols_;
    mutable ea::string lastError_;
};

} // namespace Urho3D
