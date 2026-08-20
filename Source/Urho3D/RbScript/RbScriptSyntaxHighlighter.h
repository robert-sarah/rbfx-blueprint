// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include "RbScriptDefs.h"

namespace Urho3D
{

enum class RbScriptSyntaxCategory
{
    Declaration,
    ControlFlow,
    Modifier,
    Async,
    Literal,
    Number,
    String,
    Type,
    Function,
    Identifier,
    Operator,
    Punctuation,
    Comment,
    Invalid,
};

/// Classify a token for editor presentation without changing parser semantics.
/// `callSite` is true when an identifier is immediately followed by `(`.
URHO3D_API RbScriptSyntaxCategory ClassifyRbScriptSyntax(
    RbScriptTokenKind kind, const ea::string& lexeme, bool callSite = false);

URHO3D_API ea::string ToString(RbScriptSyntaxCategory category);

} // namespace Urho3D
