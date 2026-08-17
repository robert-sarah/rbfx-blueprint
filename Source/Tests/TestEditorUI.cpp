// SPDX-License-Identifier: MIT

#include <catch2/catch_amalgamated.hpp>

#include <Urho3D/Blueprint/BlueprintGraph.h>
#include <Urho3D/Core/EditorUIContracts.h>
#include <Urho3D/RbScript/RbScriptEditorContract.h>
#include <Urho3D/RbScript/RbScriptLexer.h>
#include <Urho3D/RbScript/RbScriptParser.h>

#include "../Editor/Core/EditorWorkspace.h"

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

TEST_CASE("editor workspace persistence contract enumerates stable presets", "[editor][ui-contract][workspace]")
{
    const auto& presets = GetEditorWorkspacePresets();
    REQUIRE(presets.size() == 11);

    for (const EditorWorkspacePreset& preset : presets)
    {
        REQUIRE(preset.name_ != nullptr);
        REQUIRE(preset.description_ != nullptr);
        REQUIRE(ea::string{GetEditorWorkspaceId(preset.id_)} == preset.name_);
        REQUIRE(ParseEditorWorkspace(preset.name_) == preset.id_);
        REQUIRE(!ea::string{GetEditorWorkspaceName(preset.id_)}.empty());
    }
    REQUIRE(ParseEditorWorkspace("unknown-workspace") == EditorWorkspace::Layout);
}

TEST_CASE("editor command palette fuzzy scoring is deterministic", "[editor][ui-contract][command-palette]")
{
    REQUIRE(MatchEditorFuzzyText("Command Palette", "cpt"));
    REQUIRE(MatchEditorFuzzyText("Command Palette", "PAL"));
    REQUIRE_FALSE(MatchEditorFuzzyText("Command Palette", "xyz"));
    REQUIRE(ScoreEditorFuzzyText("Command Palette", "com") > ScoreEditorFuzzyText("Command Palette", "cpt"));
    REQUIRE(ScoreEditorFuzzyText("Command Palette", "xyz") == -1);
}

TEST_CASE("editor asset and inspector filters are case insensitive substring contracts", "[editor][ui-contract][filter]")
{
    REQUIRE(MatchEditorTextFilter("Scripts/Player.rbscript", "player"));
    REQUIRE(MatchEditorTextFilter("TransformPosition", "FORMPOS"));
    REQUIRE(MatchEditorTextFilter("", ""));
    REQUIRE_FALSE(MatchEditorTextFilter("Scripts/Player.rbscript", "material"));
}

TEST_CASE("editor autosave manifest contract rejects unsafe or incomplete snapshots", "[editor][ui-contract][autosave]")
{
    JSONValue manifest(JSON_OBJECT);
    manifest.Set("Version", 1u);
    manifest.Set("ProjectPath", "/tmp/EditorContract/");
    manifest.Set("Timestamp", "20260817_120000");

    JSONValue unsavedItems(JSON_ARRAY);
    unsavedItems.Push("Scripts/Player.rbscript");
    unsavedItems.Push("Blueprints/Player.blueprint");
    manifest.Set("UnsavedItems", ea::move(unsavedItems));

    JSONValue files(JSON_ARRAY);
    files.Push("Project.json");
    files.Push("ui.ini");
    manifest.Set("Files", ea::move(files));

    ea::string error;
    REQUIRE(ValidateEditorAutosaveManifest(manifest, &error));
    REQUIRE(error.empty());
    REQUIRE(IsSafeEditorAutosavePath("Scripts/Player.rbscript"));
    REQUIRE_FALSE(IsSafeEditorAutosavePath("../Project.json"));
    REQUIRE_FALSE(IsSafeEditorAutosavePath("/absolute/file"));
    REQUIRE_FALSE(IsSafeEditorAutosavePath("Scripts\\Player.rbscript"));

    manifest.Set("Files", JSONValue(JSON_ARRAY));
    manifest["Files"].Push("../escape.txt");
    REQUIRE_FALSE(ValidateEditorAutosaveManifest(manifest, &error));
    REQUIRE_FALSE(error.empty());

    manifest.Erase("Timestamp");
    REQUIRE_FALSE(ValidateEditorAutosaveManifest(manifest, &error));
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
