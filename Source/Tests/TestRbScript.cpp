// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#include <catch2/catch_amalgamated.hpp>

#include <Urho3D/RbScript/RbScriptLexer.h>
#include <Urho3D/RbScript/RbScriptSyntaxHighlighter.h>

using namespace Urho3D;

TEST_CASE("rbscript lexer tokenizes typed gameplay syntax", "[rbscript][lexer]")
{
    const ea::string source =
        "module Game.Player;\n"
        "script PlayerController : NodeBehavior {\n"
        "  [[export]] f32 speed = 6.0f;\n"
        "  fn move(Vector3 direction) -> void { owner().translate(direction); }\n"
        "}\n";

    RbScriptLexer lexer(source, "Player.rbscript");
    const ea::vector<RbScriptToken> tokens = lexer.Tokenize();

    REQUIRE(lexer.GetDiagnostics().empty());
    REQUIRE(tokens.back().kind == RbScriptTokenKind::EndOfFile);
    REQUIRE(tokens[0].kind == RbScriptTokenKind::Module);
    REQUIRE(tokens[1].kind == RbScriptTokenKind::Identifier);
    REQUIRE(tokens[2].kind == RbScriptTokenKind::Dot);
    REQUIRE(tokens[3].kind == RbScriptTokenKind::Identifier);
    REQUIRE(tokens[4].kind == RbScriptTokenKind::Semicolon);

    bool foundFloat = false;
    bool foundArrow = false;
    bool foundAttribute = false;
    for (const RbScriptToken& token : tokens)
    {
        foundFloat |= token.kind == RbScriptTokenKind::FloatLiteral && token.lexeme == "6.0f";
        foundArrow |= token.kind == RbScriptTokenKind::Arrow;
        foundAttribute |= token.kind == RbScriptTokenKind::LeftBracket;
    }
    REQUIRE(foundFloat);
    REQUIRE(foundArrow);
    REQUIRE(foundAttribute);
}

TEST_CASE("rbscript lexer handles comments and escaped strings", "[rbscript][lexer]")
{
    const ea::string source =
        "// line comment\n"
        "/* block comment */\n"
        "fn text() -> String { return \"hello\\nworld\\\"\"; }\n";

    RbScriptLexer lexer(source);
    const ea::vector<RbScriptToken> tokens = lexer.Tokenize();

    REQUIRE(lexer.GetDiagnostics().empty());
    REQUIRE(tokens.size() >= 9);

    bool foundString = false;
    for (const RbScriptToken& token : tokens)
    {
        if (token.kind == RbScriptTokenKind::StringLiteral)
        {
            foundString = true;
            REQUIRE(token.lexeme == "hello\nworld\"");
        }
    }
    REQUIRE(foundString);
}

TEST_CASE("rbscript syntax highlighter classifies token families", "[rbscript][editor][syntax]")
{
    CHECK(ClassifyRbScriptSyntax(RbScriptTokenKind::Script, "script") == RbScriptSyntaxCategory::Declaration);
    CHECK(ClassifyRbScriptSyntax(RbScriptTokenKind::If, "if") == RbScriptSyntaxCategory::ControlFlow);
    CHECK(ClassifyRbScriptSyntax(RbScriptTokenKind::Const, "const") == RbScriptSyntaxCategory::Modifier);
    CHECK(ClassifyRbScriptSyntax(RbScriptTokenKind::Await, "await") == RbScriptSyntaxCategory::Async);
    CHECK(ClassifyRbScriptSyntax(RbScriptTokenKind::IntegerLiteral, "42") == RbScriptSyntaxCategory::Number);
    CHECK(ClassifyRbScriptSyntax(RbScriptTokenKind::StringLiteral, "text") == RbScriptSyntaxCategory::String);
    CHECK(ClassifyRbScriptSyntax(RbScriptTokenKind::Identifier, "Vector3") == RbScriptSyntaxCategory::Type);
    CHECK(ClassifyRbScriptSyntax(RbScriptTokenKind::Identifier, "translate", true) == RbScriptSyntaxCategory::Function);
    CHECK(ClassifyRbScriptSyntax(RbScriptTokenKind::Plus, "+") == RbScriptSyntaxCategory::Operator);
    CHECK(ClassifyRbScriptSyntax(RbScriptTokenKind::LeftBrace, "{") == RbScriptSyntaxCategory::Punctuation);
    CHECK(ToString(RbScriptSyntaxCategory::ControlFlow) == "control-flow");
}

TEST_CASE("rbscript syntax spans preserve source spelling for highlighting", "[rbscript][editor][syntax]")
{
    const ea::string source = "fn draw() { return \"hello\\nworld\"; } // frame\n";
    RbScriptLexer lexer(source, "Highlight.rbscript");
    const ea::vector<RbScriptToken> tokens = lexer.Tokenize();
    REQUIRE(lexer.GetDiagnostics().empty());

    bool foundString = false;
    for (const RbScriptToken& token : tokens)
    {
        if (token.kind != RbScriptTokenKind::StringLiteral)
            continue;
        const ea::string spelling = source.substr(token.span.begin.offset,
            token.span.end.offset - token.span.begin.offset);
        CHECK(spelling == "\"hello\\nworld\"");
        foundString = true;
    }
    CHECK(foundString);
}

TEST_CASE("rbscript lexer reports malformed source with locations", "[rbscript][lexer][diagnostics]")
{
    RbScriptLexer lexer("fn broken() -> String { return \"unterminated; @ }");
    const ea::vector<RbScriptToken> tokens = lexer.Tokenize();

    REQUIRE(!tokens.empty());
    REQUIRE(!lexer.GetDiagnostics().empty());
    REQUIRE(lexer.GetDiagnostics()[0].severity == RbScriptDiagnosticSeverity::Error);
    REQUIRE(lexer.GetDiagnostics()[0].span.begin.line == 1);
    REQUIRE(lexer.GetDiagnostics()[0].span.begin.column > 1);
}
