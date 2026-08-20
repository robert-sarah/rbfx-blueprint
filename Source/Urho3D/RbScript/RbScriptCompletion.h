// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <utility>
#include <vector>

namespace Urho3D
{

enum class RbScriptCompletionKind
{
    Keyword,
    Type,
    Function,
    Variable,
    Member,
};

enum class RbScriptCompletionLocation
{
    Local,
    Parent,
    Global,
};

struct RbScriptCompletionItem
{
    std::string label;
    std::string detail;
    RbScriptCompletionKind kind{RbScriptCompletionKind::Keyword};
    RbScriptCompletionLocation location{RbScriptCompletionLocation::Global};
    int score{};
};

inline std::string RbScriptCompletionLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

inline std::vector<RbScriptCompletionItem> RankRbScriptCompletions(
    const std::string& prefix, const std::vector<RbScriptCompletionItem>& candidates, unsigned limit = 48)
{
    const std::string query = RbScriptCompletionLower(prefix);
    std::vector<RbScriptCompletionItem> ranked;
    ranked.reserve(candidates.size());

    for (RbScriptCompletionItem candidate : candidates)
    {
        const std::string label = RbScriptCompletionLower(candidate.label);
        if (!query.empty() && label.find(query) == std::string::npos)
            continue;

        const bool exact = label == query;
        const bool prefixMatch = !query.empty() && label.rfind(query, 0) == 0;
        const int locationScore = candidate.location == RbScriptCompletionLocation::Local ? 0
            : candidate.location == RbScriptCompletionLocation::Parent ? 100 : 200;
        const int matchScore = query.empty() ? 50 : exact ? 0 : prefixMatch ? 10 : 20;
        candidate.score = matchScore * 1000 + locationScore;
        ranked.push_back(std::move(candidate));
    }

    std::stable_sort(ranked.begin(), ranked.end(), [](const RbScriptCompletionItem& left,
                                                        const RbScriptCompletionItem& right) {
        if (left.score != right.score)
            return left.score < right.score;
        if (left.label.size() != right.label.size())
            return left.label.size() < right.label.size();
        if (left.kind != right.kind)
            return static_cast<int>(left.kind) < static_cast<int>(right.kind);
        return RbScriptCompletionLower(left.label) < RbScriptCompletionLower(right.label);
    });

    if (ranked.size() > limit)
        ranked.resize(limit);
    return ranked;
}

inline const char* GetRbScriptCompletionKindName(RbScriptCompletionKind kind)
{
    switch (kind)
    {
    case RbScriptCompletionKind::Keyword: return "keyword";
    case RbScriptCompletionKind::Type: return "type";
    case RbScriptCompletionKind::Function: return "function";
    case RbScriptCompletionKind::Variable: return "variable";
    case RbScriptCompletionKind::Member: return "member";
    default: return "symbol";
    }
}

} // namespace Urho3D
