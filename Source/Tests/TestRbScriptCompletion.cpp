// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#include <Urho3D/RbScript/RbScriptCompletion.h>

#include "CommonUtils.h"

using namespace Urho3D;

TEST_CASE("RbScript completion prefers local symbols and supports case-insensitive prefix search", "[rbscript][editor]")
{
    const std::vector<RbScriptCompletionItem> candidates = {
        {"Health", "local field", RbScriptCompletionKind::Variable, RbScriptCompletionLocation::Local},
        {"health", "global function", RbScriptCompletionKind::Function, RbScriptCompletionLocation::Global},
        {"Heartbeat", "global function", RbScriptCompletionKind::Function, RbScriptCompletionLocation::Global},
    };

    const std::vector<RbScriptCompletionItem> ranked = RankRbScriptCompletions("HEA", candidates);
    REQUIRE(ranked.size() == 3);
    CHECK(ranked[0].label == "Health");
    CHECK(ranked[0].score < ranked[1].score);
    CHECK(ranked[1].label == "health");
}

TEST_CASE("RbScript completion ranks exact labels before substring matches and honors limits", "[rbscript][editor]")
{
    const std::vector<RbScriptCompletionItem> candidates = {
        {"tick", "keyword", RbScriptCompletionKind::Keyword, RbScriptCompletionLocation::Global},
        {"TickRate", "type", RbScriptCompletionKind::Type, RbScriptCompletionLocation::Global},
        {"network_tick", "member", RbScriptCompletionKind::Member, RbScriptCompletionLocation::Parent},
    };

    const std::vector<RbScriptCompletionItem> ranked = RankRbScriptCompletions("tick", candidates, 2);
    REQUIRE(ranked.size() == 2);
    CHECK(ranked[0].label == "tick");
    CHECK(ranked[0].score == 200);
    CHECK(ranked[1].label == "TickRate");
    CHECK(ranked[1].score > ranked[0].score);
}

TEST_CASE("RbScript completion exposes stable semantic kind names", "[rbscript][editor]")
{
    CHECK(std::string(GetRbScriptCompletionKindName(RbScriptCompletionKind::Keyword)) == "keyword");
    CHECK(std::string(GetRbScriptCompletionKindName(RbScriptCompletionKind::Type)) == "type");
    CHECK(std::string(GetRbScriptCompletionKindName(RbScriptCompletionKind::Function)) == "function");
    CHECK(std::string(GetRbScriptCompletionKindName(RbScriptCompletionKind::Variable)) == "variable");
}
