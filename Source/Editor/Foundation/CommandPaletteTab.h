// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include "../Project/EditorTab.h"

namespace Urho3D
{

class Project;

struct CommandPaletteCommand
{
    ea::string id_;
    ea::string label_;
    ea::string category_;
    ea::string shortcut_;
    ea::function<void(Project*)> execute_;
};

/// Global searchable command surface for editor workflows.
/// Commands can be registered by Foundation tabs or external editor plugins.
class CommandPaletteTab : public EditorTab
{
    URHO3D_OBJECT(CommandPaletteTab, EditorTab);

public:
    explicit CommandPaletteTab(Context* context);

    /// Register or replace a command in the global palette registry.
    static void RegisterCommand(const CommandPaletteCommand& command);
    /// Remove all commands owned by a plugin or subsystem prefix.
    static void UnregisterCommands(const ea::string& idPrefix);

    void PreRenderUpdate() override;
    void RenderContent() override;
    void RenderMenu() override;
    void WriteIniSettings(ImGuiTextBuffer& output) override;
    void ReadIniSettings(const char* line) override;

    /// Open the palette and focus its search field.
    void OpenPalette();

private:
    void ExecuteSelected();
    void RebuildResults();
    static bool MatchesQuery(const CommandPaletteCommand& command, const ea::string& query);
    static int ScoreMatch(const ea::string& text, const ea::string& query);

    ea::string query_;
    ea::vector<unsigned> resultIndices_;
    ea::vector<ea::string> recentCommandIds_;
    unsigned selectedResult_{};
    bool focusSearch_{};
    bool paletteVisible_{};
};

/// Foundation plugin entry point.
void Foundation_CommandPaletteTab(Context* context, Project* project);

}
