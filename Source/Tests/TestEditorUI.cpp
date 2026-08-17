// SPDX-License-Identifier: MIT

#include <catch2/catch_amalgamated.hpp>

#include <Urho3D/Blueprint/BlueprintGraph.h>
#include <Urho3D/RbScript/RbScriptEditorContract.h>
#include <Urho3D/RbScript/RbScriptLexer.h>
#include <Urho3D/RbScript/RbScriptParser.h>

using namespace Urho3D;

namespace
{

RbScriptModule ParseEditorTemplate(const ea::string& source)
{
    RbScriptLexer lexer(source, "EditorTemplate.rbscript");
    const ea::vector<RbScriptToken> tokens = lexer.Tokenize();
    REQUIRE(lexer.GetDiagnostics().empty());
    RbScriptParser parser(tokens, "EditorTemplate.rbscript");
    RbScriptModule module = parser.ParseModule();
    REQUIRE(parser.GetDiagnostics().empty());
    return module;
}

}

TEST_CASE("editor resource routing accepts rbscript and rejects scene", "[editor][ui-contract]")
{
    REQUIRE(RbScriptEditorContract::IsRbScriptResourcePath("Scripts/Player.rbscript"));
    REQUIRE(RbScriptEditorContract::IsRbScriptResourcePath("Player.rbscript"));
    REQUIRE_FALSE(RbScriptEditorContract::IsRbScriptResourcePath("Scenes/Main.scene"));
    REQUIRE_FALSE(RbScriptEditorContract::IsRbScriptResourcePath("Scripts/Player.rbscript.bak"));
}

TEST_CASE("editor rbscript templates create parseable typed modules", "[editor][ui-contract][rbscript]")
{
    for (unsigned templateIndex = 0; templateIndex < 4; ++templateIndex)
    {
        const ea::string source = RbScriptEditorContract::GetTemplateSource(templateIndex);
        REQUIRE(source.find("script ") != ea::string::npos);
        REQUIRE(source.back() == '\n');
        if (templateIndex == 0)
        {
            RbScriptLexer lexer(source, "DefaultTemplate.rbscript");
            REQUIRE(lexer.Tokenize().back().kind == RbScriptTokenKind::EndOfFile);
            REQUIRE(lexer.GetDiagnostics().empty());
            continue;
        }

        const RbScriptModule module = ParseEditorTemplate(source);
        REQUIRE(module.IsValid());
        REQUIRE(module.scripts.size() == 1);
        REQUIRE(!module.scripts.front().name.empty());
    }
}

TEST_CASE("editor blueprint deletion workflow preserves an undoable snapshot", "[editor][ui-contract][blueprint]")
{
    BlueprintGraph graph("EditorDeletionContract");
    const BlueprintId nodeId = graph.AddNode("Flow.Print", "Print", {0.0f, 0.0f});
    REQUIRE(nodeId != BLUEPRINT_INVALID_ID);
    const ea::string before = graph.ToString();

    REQUIRE(graph.RemoveNode(nodeId));
    const ea::string after = graph.ToString();
    REQUIRE(before != after);

    BlueprintGraph restored;
    ea::string error;
    REQUIRE(restored.FromString(before, &error));
    REQUIRE(error.empty());
    REQUIRE(restored.GetNode(nodeId) != nullptr);
    REQUIRE(restored.RemoveNode(nodeId));
}

TEST_CASE("editor save and reload contract keeps rbscript source stable", "[editor][ui-contract][rbscript]")
{
    const ea::string source = RbScriptEditorContract::GetTemplateSource(2);
    RbScriptLexer lexer(source, "SaveReload.rbscript");
    const ea::vector<RbScriptToken> tokens = lexer.Tokenize();
    REQUIRE(lexer.GetDiagnostics().empty());

    RbScriptParser parser(tokens, "SaveReload.rbscript");
    const RbScriptModule module = parser.ParseModule();
    REQUIRE(parser.GetDiagnostics().empty());
    REQUIRE(module.IsValid());
    REQUIRE(module.scripts.front().functions.size() == 2);
    REQUIRE(module.scripts.front().functions[0].name == "on_start");
    REQUIRE(module.scripts.front().functions[1].name == "tick");
}
