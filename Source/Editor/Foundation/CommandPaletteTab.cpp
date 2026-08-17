// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#include "CommandPaletteTab.h"

#include "../Core/IniHelpers.h"
#include "../Project/Project.h"

#include <Urho3D/Core/StringUtils.h>
#include <Urho3D/SystemUI/SystemUI.h>

#include <IconFontCppHeaders/IconsFontAwesome6.h>

#include <algorithm>
#include <cctype>

namespace Urho3D
{

namespace
{

ea::vector<CommandPaletteCommand>& GetCommands()
{
    static ea::vector<CommandPaletteCommand> commands;
    return commands;
}

bool ContainsFuzzy(const ea::string& text, const ea::string& query)
{
    if (query.empty())
        return true;

    size_t queryIndex = 0;
    for (char character : text)
    {
        if (std::tolower(static_cast<unsigned char>(character)) ==
            std::tolower(static_cast<unsigned char>(query[queryIndex])))
        {
            ++queryIndex;
            if (queryIndex == query.size())
                return true;
        }
    }
    return false;
}

ea::string JoinRecent(const ea::vector<ea::string>& values)
{
    ea::string result;
    for (const ea::string& value : values)
    {
        if (!result.empty())
            result += ';';
        result += value;
    }
    return result;
}

void SplitRecent(const ea::string& value, ea::vector<ea::string>& result)
{
    result.clear();
    size_t begin = 0;
    while (begin < value.size())
    {
        const size_t end = value.find(';', begin);
        const size_t length = end == ea::string::npos ? value.size() - begin : end - begin;
        if (length > 0)
            result.emplace_back(value.substr(begin, length));
        if (end == ea::string::npos)
            break;
        begin = end + 1;
    }
}

} // namespace

CommandPaletteTab::CommandPaletteTab(Context* context)
    : EditorTab(context, "Command Palette", "c5ca5f41-7895-4fe5-98e9-e2a32e55d3ad",
        EditorTabFlag::None, EditorTabPlacement::Floating)
{
    RegisterCommand({
        "Project.Save", "Save Project", "Project", "Ctrl+Shift+S",
        [](Project* project) { project->Save(); }
    });
    RegisterCommand({
        "Workspace.Layout", "Switch Workspace: Layout", "Workspace", "",
        [](Project* project) { project->SetWorkspace(EditorWorkspace::Layout); }
    });
    RegisterCommand({
        "Workspace.Scene2D", "Switch Workspace: Scene 2D", "Workspace", "",
        [](Project* project) { project->SetWorkspace(EditorWorkspace::Scene2D); }
    });
    RegisterCommand({
        "Workspace.Scene3D", "Switch Workspace: Scene 3D", "Workspace", "",
        [](Project* project) { project->SetWorkspace(EditorWorkspace::Scene3D); }
    });
    RegisterCommand({
        "Workspace.Blueprint", "Switch Workspace: Blueprint", "Workspace", "",
        [](Project* project) { project->SetWorkspace(EditorWorkspace::Blueprint); }
    });
    RegisterCommand({
        "Workspace.Scripting", "Switch Workspace: Scripting", "Workspace", "",
        [](Project* project) { project->SetWorkspace(EditorWorkspace::Scripting); }
    });
    RegisterCommand({
        "Workspace.WorldFabric", "Switch Workspace: World Fabric", "Workspace", "",
        [](Project* project) { project->SetWorkspace(EditorWorkspace::WorldFabric); }
    });
    RegisterCommand({
        "Workspace.Build", "Switch Workspace: Build", "Workspace", "",
        [](Project* project) { project->SetWorkspace(EditorWorkspace::Build); }
    });
}

void CommandPaletteTab::RegisterCommand(const CommandPaletteCommand& command)
{
    if (command.id_.empty() || !command.execute_)
        return;

    auto& commands = GetCommands();
    const auto existing = ea::find_if(commands.begin(), commands.end(), [&](const CommandPaletteCommand& item)
    {
        return item.id_ == command.id_;
    });
    if (existing != commands.end())
        *existing = command;
    else
        commands.push_back(command);
}

void CommandPaletteTab::UnregisterCommands(const ea::string& idPrefix)
{
    auto& commands = GetCommands();
    ea::erase_if(commands, [&](const CommandPaletteCommand& command)
    {
        return command.id_.starts_with(idPrefix);
    });
}

void CommandPaletteTab::OpenPalette()
{
    paletteVisible_ = true;
    focusSearch_ = true;
    query_.clear();
    selectedResult_ = 0;
    RebuildResults();
    Open();
    Focus(true);
}

void CommandPaletteTab::PreRenderUpdate()
{
    if (!paletteVisible_)
        return;

    if (ui::IsKeyPressed(KEY_ESCAPE))
    {
        paletteVisible_ = false;
        Close();
    }
    else if (ui::IsKeyPressed(KEY_DOWN) && !resultIndices_.empty())
        selectedResult_ = ea::min(selectedResult_ + 1u, static_cast<unsigned>(resultIndices_.size() - 1));
    else if (ui::IsKeyPressed(KEY_UP) && selectedResult_ > 0)
        --selectedResult_;
    else if (ui::IsKeyPressed(KEY_RETURN))
        ExecuteSelected();
}

void CommandPaletteTab::RenderContent()
{
    if (!paletteVisible_)
        return;

    ui::SetNextWindowSize(ImVec2{640.0f, 420.0f}, ImGuiCond_FirstUseEver);
    ui::TextUnformatted("Search editor commands");
    ui::SetNextItemWidth(-1.0f);
    if (focusSearch_)
    {
        ui::SetKeyboardFocusHere();
        focusSearch_ = false;
    }
    if (ui::InputText("##CommandPaletteSearch", &query_))
    {
        selectedResult_ = 0;
        RebuildResults();
    }

    ui::Separator();
    if (resultIndices_.empty())
    {
        ui::TextDisabled("No matching commands");
        return;
    }

    const auto& commands = GetCommands();
    for (unsigned resultIndex = 0; resultIndex < resultIndices_.size(); ++resultIndex)
    {
        const CommandPaletteCommand& command = commands[resultIndices_[resultIndex]];
        const bool selected = resultIndex == selectedResult_;
        ea::string label = command.category_ + "  /  " + command.label_;
        if (!command.shortcut_.empty())
            label += "    [" + command.shortcut_ + "]";
        if (ui::Selectable(label.c_str(), selected))
        {
            selectedResult_ = resultIndex;
            ExecuteSelected();
            break;
        }
        if (selected)
            ui::SetItemDefaultFocus();
    }
}

void CommandPaletteTab::RenderMenu()
{
    if (ui::MenuItem("Command Palette", "Ctrl+P"))
        OpenPalette();
}

void CommandPaletteTab::ExecuteSelected()
{
    if (resultIndices_.empty() || selectedResult_ >= resultIndices_.size())
        return;

    const CommandPaletteCommand& command = GetCommands()[resultIndices_[selectedResult_]];
    if (Project* project = GetProject())
    {
        command.execute_(project);
        recentCommandIds_.erase(ea::remove(recentCommandIds_.begin(), recentCommandIds_.end(), command.id_), recentCommandIds_.end());
        recentCommandIds_.insert(recentCommandIds_.begin(), command.id_);
        if (recentCommandIds_.size() > 12)
            recentCommandIds_.resize(12);
    }

    paletteVisible_ = false;
    Close();
}

void CommandPaletteTab::RebuildResults()
{
    resultIndices_.clear();
    const auto& commands = GetCommands();
    for (unsigned index = 0; index < commands.size(); ++index)
    {
        if (MatchesQuery(commands[index], query_))
            resultIndices_.push_back(index);
    }

    ea::stable_sort(resultIndices_.begin(), resultIndices_.end(), [&](unsigned lhs, unsigned rhs)
    {
        const auto score = [&](unsigned index)
        {
            const CommandPaletteCommand& command = commands[index];
            return ScoreMatch(command.label_, query_) + ScoreMatch(command.category_, query_);
        };
        return score(lhs) > score(rhs);
    });
    if (selectedResult_ >= resultIndices_.size())
        selectedResult_ = resultIndices_.empty() ? 0 : static_cast<unsigned>(resultIndices_.size() - 1);
}

bool CommandPaletteTab::MatchesQuery(const CommandPaletteCommand& command, const ea::string& query)
{
    return ContainsFuzzy(command.label_, query) || ContainsFuzzy(command.category_, query) || ContainsFuzzy(command.id_, query);
}

int CommandPaletteTab::ScoreMatch(const ea::string& text, const ea::string& query)
{
    if (query.empty())
        return 0;

    int score = 0;
    size_t queryIndex = 0;
    bool contiguous = true;
    for (char character : text)
    {
        if (queryIndex >= query.size())
            break;
        if (std::tolower(static_cast<unsigned char>(character)) ==
            std::tolower(static_cast<unsigned char>(query[queryIndex])))
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

void CommandPaletteTab::WriteIniSettings(ImGuiTextBuffer& output)
{
    EditorTab::WriteIniSettings(output);
    WriteStringToIni(output, "Recent", JoinRecent(recentCommandIds_));
}

void CommandPaletteTab::ReadIniSettings(const char* line)
{
    EditorTab::ReadIniSettings(line);
    if (const auto value = ReadStringFromIni(line, "Recent"))
        SplitRecent(*value, recentCommandIds_);
}

void Foundation_CommandPaletteTab(Context* context, Project* project)
{
    project->AddTab(MakeShared<CommandPaletteTab>(context));
}

}
