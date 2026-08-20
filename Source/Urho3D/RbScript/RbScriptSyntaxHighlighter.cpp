// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#include "RbScriptSyntaxHighlighter.h"

namespace Urho3D
{

namespace
{

bool IsDeclaration(RbScriptTokenKind kind)
{
    switch (kind)
    {
    case RbScriptTokenKind::Module:
    case RbScriptTokenKind::Use:
    case RbScriptTokenKind::Script:
    case RbScriptTokenKind::Fn:
    case RbScriptTokenKind::Struct:
    case RbScriptTokenKind::Enum:
    case RbScriptTokenKind::Class:
    case RbScriptTokenKind::Signal:
        return true;
    default:
        return false;
    }
}

bool IsControlFlow(RbScriptTokenKind kind)
{
    switch (kind)
    {
    case RbScriptTokenKind::Return:
    case RbScriptTokenKind::If:
    case RbScriptTokenKind::Else:
    case RbScriptTokenKind::While:
    case RbScriptTokenKind::For:
    case RbScriptTokenKind::In:
    case RbScriptTokenKind::Match:
    case RbScriptTokenKind::Break:
    case RbScriptTokenKind::Continue:
        return true;
    default:
        return false;
    }
}

bool IsModifier(RbScriptTokenKind kind)
{
    switch (kind)
    {
    case RbScriptTokenKind::Let:
    case RbScriptTokenKind::Var:
    case RbScriptTokenKind::Const:
    case RbScriptTokenKind::Public:
    case RbScriptTokenKind::Private:
    case RbScriptTokenKind::Static:
        return true;
    default:
        return false;
    }
}

bool IsOperator(RbScriptTokenKind kind)
{
    switch (kind)
    {
    case RbScriptTokenKind::Arrow:
    case RbScriptTokenKind::Plus:
    case RbScriptTokenKind::Minus:
    case RbScriptTokenKind::Star:
    case RbScriptTokenKind::Slash:
    case RbScriptTokenKind::Percent:
    case RbScriptTokenKind::Equal:
    case RbScriptTokenKind::EqualEqual:
    case RbScriptTokenKind::Bang:
    case RbScriptTokenKind::BangEqual:
    case RbScriptTokenKind::Less:
    case RbScriptTokenKind::LessEqual:
    case RbScriptTokenKind::Greater:
    case RbScriptTokenKind::GreaterEqual:
    case RbScriptTokenKind::AndAnd:
    case RbScriptTokenKind::OrOr:
    case RbScriptTokenKind::Ampersand:
    case RbScriptTokenKind::Pipe:
    case RbScriptTokenKind::Question:
        return true;
    default:
        return false;
    }
}

bool IsPunctuation(RbScriptTokenKind kind)
{
    switch (kind)
    {
    case RbScriptTokenKind::LeftBrace:
    case RbScriptTokenKind::RightBrace:
    case RbScriptTokenKind::LeftParen:
    case RbScriptTokenKind::RightParen:
    case RbScriptTokenKind::LeftBracket:
    case RbScriptTokenKind::RightBracket:
    case RbScriptTokenKind::Comma:
    case RbScriptTokenKind::Dot:
    case RbScriptTokenKind::Colon:
    case RbScriptTokenKind::Semicolon:
    case RbScriptTokenKind::Scope:
    case RbScriptTokenKind::At:
        return true;
    default:
        return false;
    }
}

bool LooksLikeType(const ea::string& lexeme)
{
    if (lexeme.empty())
        return false;
    const char first = lexeme.front();
    return first >= 'A' && first <= 'Z';
}

} // namespace

RbScriptSyntaxCategory ClassifyRbScriptSyntax(RbScriptTokenKind kind, const ea::string& lexeme, bool callSite)
{
    if (IsDeclaration(kind))
        return RbScriptSyntaxCategory::Declaration;
    if (IsControlFlow(kind))
        return RbScriptSyntaxCategory::ControlFlow;
    if (IsModifier(kind))
        return RbScriptSyntaxCategory::Modifier;

    switch (kind)
    {
    case RbScriptTokenKind::On:
    case RbScriptTokenKind::Async:
    case RbScriptTokenKind::Await:
    case RbScriptTokenKind::Emit:
        return RbScriptSyntaxCategory::Async;
    case RbScriptTokenKind::True:
    case RbScriptTokenKind::False:
    case RbScriptTokenKind::Null:
        return RbScriptSyntaxCategory::Literal;
    case RbScriptTokenKind::IntegerLiteral:
    case RbScriptTokenKind::FloatLiteral:
        return RbScriptSyntaxCategory::Number;
    case RbScriptTokenKind::StringLiteral:
        return RbScriptSyntaxCategory::String;
    case RbScriptTokenKind::Identifier:
        if (callSite)
            return RbScriptSyntaxCategory::Function;
        return LooksLikeType(lexeme) ? RbScriptSyntaxCategory::Type : RbScriptSyntaxCategory::Identifier;
    case RbScriptTokenKind::Invalid:
        return RbScriptSyntaxCategory::Invalid;
    default:
        if (IsOperator(kind))
            return RbScriptSyntaxCategory::Operator;
        if (IsPunctuation(kind))
            return RbScriptSyntaxCategory::Punctuation;
        return RbScriptSyntaxCategory::Identifier;
    }
}

ea::string ToString(RbScriptSyntaxCategory category)
{
    switch (category)
    {
    case RbScriptSyntaxCategory::Declaration: return "declaration";
    case RbScriptSyntaxCategory::ControlFlow: return "control-flow";
    case RbScriptSyntaxCategory::Modifier: return "modifier";
    case RbScriptSyntaxCategory::Async: return "async";
    case RbScriptSyntaxCategory::Literal: return "literal";
    case RbScriptSyntaxCategory::Number: return "number";
    case RbScriptSyntaxCategory::String: return "string";
    case RbScriptSyntaxCategory::Type: return "type";
    case RbScriptSyntaxCategory::Function: return "function";
    case RbScriptSyntaxCategory::Identifier: return "identifier";
    case RbScriptSyntaxCategory::Operator: return "operator";
    case RbScriptSyntaxCategory::Punctuation: return "punctuation";
    case RbScriptSyntaxCategory::Comment: return "comment";
    case RbScriptSyntaxCategory::Invalid: return "invalid";
    default: return "unknown";
    }
}

} // namespace Urho3D
