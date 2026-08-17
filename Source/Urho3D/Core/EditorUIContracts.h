// SPDX-License-Identifier: MIT

#pragma once

#include <EASTL/string.h>

#include <Urho3D/Resource/JSONValue.h>

namespace Urho3D
{

/// Return whether query characters occur in order in text, case-insensitively.
URHO3D_API bool MatchEditorFuzzyText(const ea::string& text, const ea::string& query);

/// Score a fuzzy match. Higher values prefer contiguous characters; -1 means no match.
URHO3D_API int ScoreEditorFuzzyText(const ea::string& text, const ea::string& query);

/// Return whether query is a case-insensitive substring of text.
URHO3D_API bool MatchEditorTextFilter(const ea::string& text, const ea::string& query);

/// Validate the stable, relative paths accepted by an autosave manifest.
URHO3D_API bool IsSafeEditorAutosavePath(const ea::string& path);

/// Validate the versioned autosave manifest contract before recovery uses it.
URHO3D_API bool ValidateEditorAutosaveManifest(const JSONValue& root, ea::string* error = nullptr);

} // namespace Urho3D
