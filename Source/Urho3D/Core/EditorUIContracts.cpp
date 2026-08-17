// SPDX-License-Identifier: MIT

#include "EditorUIContracts.h"

#include <Urho3D/Core/StringUtils.h>

#include <cctype>

namespace Urho3D
{

namespace
{

char ToLowerAscii(char value)
{
    return static_cast<char>(std::tolower(static_cast<unsigned char>(value)));
}

void SetValidationError(ea::string* error, const ea::string& message)
{
    if (error)
        *error = message;
}

bool ValidateStringArray(const JSONValue& value, const char* fieldName, ea::string* error)
{
    if (!value.IsArray())
    {
        SetValidationError(error, Format("{} must be an array", fieldName));
        return false;
    }

    for (const JSONValue& item : value.GetArray())
    {
        if (!item.IsString() || !IsSafeEditorAutosavePath(item.GetString()))
        {
            SetValidationError(error, Format("{} contains an unsafe relative path", fieldName));
            return false;
        }
    }
    return true;
}

} // namespace

bool MatchEditorFuzzyText(const ea::string& text, const ea::string& query)
{
    if (query.empty())
        return true;

    size_t queryIndex = 0;
    for (const char character : text)
    {
        if (ToLowerAscii(character) == ToLowerAscii(query[queryIndex]))
        {
            ++queryIndex;
            if (queryIndex == query.size())
                return true;
        }
    }
    return false;
}

int ScoreEditorFuzzyText(const ea::string& text, const ea::string& query)
{
    if (query.empty())
        return 0;

    int score = 0;
    size_t queryIndex = 0;
    bool contiguous = true;
    for (const char character : text)
    {
        if (queryIndex >= query.size())
            break;
        if (ToLowerAscii(character) == ToLowerAscii(query[queryIndex]))
        {
            score += contiguous ? 3 : 1;
            contiguous = true;
            ++queryIndex;
        }
        else
            contiguous = false;
    }
    return queryIndex == query.size() ? score : -1;
}

bool MatchEditorTextFilter(const ea::string& text, const ea::string& query)
{
    if (query.empty())
        return true;

    if (text.empty())
        return false;

    ea::string normalizedText;
    ea::string normalizedQuery;
    normalizedText.reserve(text.size());
    normalizedQuery.reserve(query.size());
    for (const char character : text)
        normalizedText += ToLowerAscii(character);
    for (const char character : query)
        normalizedQuery += ToLowerAscii(character);
    return normalizedText.find(normalizedQuery) != ea::string::npos;
}

bool IsSafeEditorAutosavePath(const ea::string& path)
{
    return !path.empty() && path.find("..") == ea::string::npos && !path.starts_with("/")
        && path.find('\\') == ea::string::npos;
}

bool ValidateEditorAutosaveManifest(const JSONValue& root, ea::string* error)
{
    if (error)
        error->clear();

    if (!root.IsObject())
    {
        SetValidationError(error, "manifest root must be an object");
        return false;
    }

    const JSONValue& version = root.Get("Version");
    if (!version.IsNumber() || version.GetUInt() != 1u)
    {
        SetValidationError(error, "manifest Version must be 1");
        return false;
    }

    const JSONValue& projectPath = root.Get("ProjectPath");
    if (!projectPath.IsString() || projectPath.GetString().empty())
    {
        SetValidationError(error, "manifest ProjectPath must be a non-empty string");
        return false;
    }

    const JSONValue& timestamp = root.Get("Timestamp");
    if (!timestamp.IsString() || timestamp.GetString().empty())
    {
        SetValidationError(error, "manifest Timestamp must be a non-empty string");
        return false;
    }

    if (!ValidateStringArray(root.Get("UnsavedItems"), "UnsavedItems", error))
        return false;
    if (!ValidateStringArray(root.Get("Files"), "Files", error))
        return false;

    return true;
}

} // namespace Urho3D
