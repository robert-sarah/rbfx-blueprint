// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#include "InteractiveDocumentation.h"

#include <algorithm>
#include <cctype>

namespace Urho3D
{

namespace
{

ea::string JoinTags(const ea::vector<ea::string>& tags)
{
    ea::string result;
    for (unsigned i = 0; i < tags.size(); ++i)
    {
        if (i)
            result += ",";
        result += tags[i];
    }
    return result;
}

} // namespace

void InteractiveDocumentation::SetError(ea::string* error, const ea::string& message) const
{
    lastError_ = message;
    if (error)
        *error = message;
}

ea::string InteractiveDocumentation::Lowercase(const ea::string& value)
{
    ea::string result = value;
    for (char& character : result)
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    return result;
}

ea::string InteractiveDocumentation::EscapeHtml(const ea::string& value)
{
    ea::string result;
    result.reserve(value.size());
    for (const char character : value)
    {
        switch (character)
        {
        case '&': result += "&amp;"; break;
        case '<': result += "&lt;"; break;
        case '>': result += "&gt;"; break;
        case '"': result += "&quot;"; break;
        case '\'': result += "&#39;"; break;
        default: result += character; break;
        }
    }
    return result;
}

unsigned InteractiveDocumentation::Score(const ea::string& haystack, const ea::string& query)
{
    if (query.empty())
        return 1;
    const ea::string lowerHaystack = Lowercase(haystack);
    const ea::string lowerQuery = Lowercase(query);
    if (lowerHaystack == lowerQuery)
        return 1000;
    if (lowerHaystack.starts_with(lowerQuery))
        return 800;
    const auto position = lowerHaystack.find(lowerQuery);
    if (position != ea::string::npos)
        return 500u - static_cast<unsigned>(std::min<size_t>(position, 400));
    return 0;
}

bool InteractiveDocumentation::AddPage(const DocumentationPage& page, ea::string* error)
{
    if (page.id.empty() || page.title.empty() || page.markdown.empty())
    {
        SetError(error, "Documentation pages require an id, title and markdown body.");
        return false;
    }
    if (FindPage(page.id))
    {
        SetError(error, Format("Documentation page '{}' already exists.", page.id));
        return false;
    }
    pages_.push_back(page);
    std::sort(pages_.begin(), pages_.end(), [](const auto& lhs, const auto& rhs) { return lhs.id < rhs.id; });
    return true;
}

bool InteractiveDocumentation::AddSymbol(const DocumentationSymbol& symbol, ea::string* error)
{
    if (symbol.name.empty() || symbol.qualifiedName.empty() || symbol.kind.empty())
    {
        SetError(error, "Documentation symbols require a name, qualified name and kind.");
        return false;
    }
    if (FindSymbol(symbol.qualifiedName))
    {
        SetError(error, Format("Documentation symbol '{}' already exists.", symbol.qualifiedName));
        return false;
    }
    symbols_.push_back(symbol);
    std::sort(symbols_.begin(), symbols_.end(), [](const auto& lhs, const auto& rhs)
    {
        return lhs.qualifiedName < rhs.qualifiedName;
    });
    return true;
}

bool InteractiveDocumentation::RemovePage(const ea::string& id, ea::string* error)
{
    auto it = std::find_if(pages_.begin(), pages_.end(), [&id](const auto& page) { return page.id == id; });
    if (it == pages_.end())
    {
        SetError(error, Format("Documentation page '{}' does not exist.", id));
        return false;
    }
    pages_.erase(it);
    return true;
}

bool InteractiveDocumentation::RemoveSymbol(const ea::string& qualifiedName, ea::string* error)
{
    auto it = std::find_if(symbols_.begin(), symbols_.end(), [&qualifiedName](const auto& symbol)
    {
        return symbol.qualifiedName == qualifiedName;
    });
    if (it == symbols_.end())
    {
        SetError(error, Format("Documentation symbol '{}' does not exist.", qualifiedName));
        return false;
    }
    symbols_.erase(it);
    return true;
}

void InteractiveDocumentation::Clear()
{
    pages_.clear();
    symbols_.clear();
    lastError_.clear();
}

void InteractiveDocumentation::IndexTypeRegistry(const RbScriptTypeRegistry& registry, const ea::string& sourceUri)
{
    for (const ea::string& name : registry.GetTypeNames())
    {
        const RbScriptType type = registry.Resolve(name);
        DocumentationSymbol symbol;
        symbol.name = name;
        symbol.qualifiedName = "rbscript.type." + name;
        symbol.kind = "type";
        symbol.signature = type.ToString();
        symbol.description = "Reflection-backed rbscript type.";
        symbol.sourceUri = sourceUri;
        symbol.since = "1.0.0";
        ea::string error;
        if (!FindSymbol(symbol.qualifiedName))
            AddSymbol(symbol, &error);
    }
    for (const ea::string& name : registry.GetFunctionNames())
    {
        const RbScriptFunctionSignature* function = registry.FindFunction(name);
        if (!function)
            continue;
        DocumentationSymbol symbol;
        symbol.name = name;
        symbol.qualifiedName = "rbscript.function." + name;
        symbol.kind = "function";
        symbol.signature = Format("fn {} -> {}", name, function->returnType.ToString());
        symbol.description = "Reflection-backed engine function callable from rbscript.";
        symbol.sourceUri = sourceUri;
        symbol.since = "1.0.0";
        ea::string error;
        if (!FindSymbol(symbol.qualifiedName))
            AddSymbol(symbol, &error);
    }
}

ea::vector<DocumentationSearchResult> InteractiveDocumentation::Search(const ea::string& query, unsigned limit) const
{
    ea::vector<DocumentationSearchResult> result;
    for (const DocumentationPage& page : pages_)
    {
        const unsigned score = std::max({Score(page.id, query), Score(page.title, query), Score(page.markdown, query), Score(JoinTags(page.tags), query)});
        if (score)
            result.push_back({page.id, page.title, "page", page.markdown.substr(0, std::min<size_t>(160, page.markdown.size())), score});
    }
    for (const DocumentationSymbol& symbol : symbols_)
    {
        const unsigned score = std::max({Score(symbol.name, query), Score(symbol.qualifiedName, query), Score(symbol.description, query)});
        if (score)
            result.push_back({symbol.qualifiedName, symbol.name, symbol.kind, symbol.description, score});
    }
    std::sort(result.begin(), result.end(), [](const auto& lhs, const auto& rhs)
    {
        if (lhs.score != rhs.score)
            return lhs.score > rhs.score;
        return lhs.id < rhs.id;
    });
    if (result.size() > limit)
        result.resize(limit);
    return result;
}

const DocumentationPage* InteractiveDocumentation::FindPage(const ea::string& id) const
{
    for (const DocumentationPage& page : pages_)
    {
        if (page.id == id)
            return &page;
    }
    return nullptr;
}

const DocumentationSymbol* InteractiveDocumentation::FindSymbol(const ea::string& qualifiedName) const
{
    for (const DocumentationSymbol& symbol : symbols_)
    {
        if (symbol.qualifiedName == qualifiedName)
            return &symbol;
    }
    return nullptr;
}

ea::string InteractiveDocumentation::RenderMarkdown(const ea::string& id) const
{
    const DocumentationPage* page = FindPage(id);
    if (!page)
        return {};
    return Format("# {}\n\n**Category:** {}\n\n{}\n", page->title, page->category, page->markdown);
}

ea::string InteractiveDocumentation::RenderHtml(const ea::string& id) const
{
    const DocumentationPage* page = FindPage(id);
    if (!page)
        return {};
    return Format("<!doctype html><html><head><meta charset=\"utf-8\"><title>{}</title></head>"
                  "<body><main><h1>{}</h1><p class=\"category\">{}</p><pre>{}</pre></main></body></html>",
        EscapeHtml(page->title), EscapeHtml(page->title), EscapeHtml(page->category), EscapeHtml(page->markdown));
}

JSONValue InteractiveDocumentation::ToJSON() const
{
    JSONValue root(JSON_OBJECT);
    root.Set("version", 1u);
    JSONValue pages(JSON_ARRAY);
    for (const DocumentationPage& page : pages_)
    {
        JSONValue value(JSON_OBJECT);
        value.Set("id", page.id);
        value.Set("title", page.title);
        value.Set("category", page.category);
        value.Set("markdown", page.markdown);
        JSONValue tags(JSON_ARRAY);
        ea::vector<ea::string> sortedTags = page.tags;
        std::sort(sortedTags.begin(), sortedTags.end());
        for (const auto& tag : sortedTags)
            tags.Push(tag);
        value.Set("tags", ea::move(tags));
        pages.Push(ea::move(value));
    }
    root.Set("pages", ea::move(pages));
    JSONValue symbols(JSON_ARRAY);
    for (const DocumentationSymbol& symbol : symbols_)
    {
        JSONValue value(JSON_OBJECT);
        value.Set("name", symbol.name);
        value.Set("qualifiedName", symbol.qualifiedName);
        value.Set("kind", symbol.kind);
        value.Set("signature", symbol.signature);
        value.Set("description", symbol.description);
        value.Set("sourceUri", symbol.sourceUri);
        value.Set("since", symbol.since);
        symbols.Push(ea::move(value));
    }
    root.Set("symbols", ea::move(symbols));
    return root;
}

bool InteractiveDocumentation::FromJSON(const JSONValue& root, ea::string* error)
{
    if (!root.IsObject() || !root.Get("version").IsNumber() || root.Get("version").GetUInt() != 1u
        || !root.Get("pages").IsArray() || !root.Get("symbols").IsArray())
    {
        SetError(error, "Documentation index requires version 1, pages and symbols arrays.");
        return false;
    }
    InteractiveDocumentation candidate;
    for (const JSONValue& value : root.Get("pages").GetArray())
    {
        if (!value.IsObject() || !value.Get("id").IsString() || !value.Get("title").IsString()
            || !value.Get("category").IsString() || !value.Get("markdown").IsString() || !value.Get("tags").IsArray())
        {
            SetError(error, "Documentation index contains an invalid page.");
            return false;
        }
        DocumentationPage page;
        page.id = value.Get("id").GetString();
        page.title = value.Get("title").GetString();
        page.category = value.Get("category").GetString();
        page.markdown = value.Get("markdown").GetString();
        for (const JSONValue& tag : value.Get("tags").GetArray())
        {
            if (!tag.IsString())
            {
                SetError(error, "Documentation page tags must be strings.");
                return false;
            }
            page.tags.push_back(tag.GetString());
        }
        if (!candidate.AddPage(page, error))
            return false;
    }
    for (const JSONValue& value : root.Get("symbols").GetArray())
    {
        if (!value.IsObject() || !value.Get("name").IsString() || !value.Get("qualifiedName").IsString()
            || !value.Get("kind").IsString() || !value.Get("signature").IsString() || !value.Get("description").IsString()
            || !value.Get("sourceUri").IsString() || !value.Get("since").IsString())
        {
            SetError(error, "Documentation index contains an invalid symbol.");
            return false;
        }
        DocumentationSymbol symbol;
        symbol.name = value.Get("name").GetString();
        symbol.qualifiedName = value.Get("qualifiedName").GetString();
        symbol.kind = value.Get("kind").GetString();
        symbol.signature = value.Get("signature").GetString();
        symbol.description = value.Get("description").GetString();
        symbol.sourceUri = value.Get("sourceUri").GetString();
        symbol.since = value.Get("since").GetString();
        if (!candidate.AddSymbol(symbol, error))
            return false;
    }
    if (!candidate.Validate(error))
        return false;
    pages_ = ea::move(candidate.pages_);
    symbols_ = ea::move(candidate.symbols_);
    return true;
}

bool InteractiveDocumentation::Validate(ea::string* error) const
{
    for (unsigned i = 0; i < pages_.size(); ++i)
    {
        if (pages_[i].id.empty() || pages_[i].title.empty() || pages_[i].markdown.empty())
        {
            SetError(error, "Documentation pages cannot contain empty required fields.");
            return false;
        }
        for (unsigned j = i + 1; j < pages_.size(); ++j)
        {
            if (pages_[i].id == pages_[j].id)
            {
                SetError(error, Format("Documentation page '{}' is duplicated.", pages_[i].id));
                return false;
            }
        }
    }
    for (unsigned i = 0; i < symbols_.size(); ++i)
    {
        if (symbols_[i].name.empty() || symbols_[i].qualifiedName.empty() || symbols_[i].kind.empty())
        {
            SetError(error, "Documentation symbols cannot contain empty required fields.");
            return false;
        }
        for (unsigned j = i + 1; j < symbols_.size(); ++j)
        {
            if (symbols_[i].qualifiedName == symbols_[j].qualifiedName)
            {
                SetError(error, Format("Documentation symbol '{}' is duplicated.", symbols_[i].qualifiedName));
                return false;
            }
        }
    }
    return true;
}

unsigned long long InteractiveDocumentation::ComputeDigest(ea::string* error) const
{
    if (!Validate(error))
        return 0;

    ea::string serialized = "documentation-v1\\n";
    for (const DocumentationPage& page : pages_)
    {
        serialized += "page|" + page.id + "|" + page.title + "|" + page.category + "|" + page.markdown + "|";
        ea::vector<ea::string> tags = page.tags;
        std::sort(tags.begin(), tags.end());
        serialized += JoinTags(tags) + "\\n";
    }
    for (const DocumentationSymbol& symbol : symbols_)
    {
        serialized += "symbol|" + symbol.name + "|" + symbol.qualifiedName + "|" + symbol.kind + "|"
            + symbol.signature + "|" + symbol.description + "|" + symbol.sourceUri + "|" + symbol.since + "\\n";
    }

    unsigned long long digest = 1469598103934665603ull;
    for (const unsigned char character : serialized)
    {
        digest ^= character;
        digest *= 1099511628211ull;
    }
    return digest;
}

} // namespace Urho3D
