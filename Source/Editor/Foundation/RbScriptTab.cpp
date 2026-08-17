// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#include "RbScriptTab.h"

#include "../Core/IniHelpers.h"
#include <Urho3D/RbScript/RbScriptEditorContract.h>
#include "ResourceBrowserTab.h"

#include <Urho3D/Core/StringUtils.h>
#include <Urho3D/IO/File.h>
#include <Urho3D/IO/FileSystem.h>
#include <Urho3D/IO/Log.h>
#include <Urho3D/Resource/ResourceCache.h>
#include <Urho3D/RbScript/RbScriptLexer.h>
#include <Urho3D/RbScript/RbScriptParser.h>
#include <Urho3D/SystemUI/Widgets.h>

namespace Urho3D
{

namespace
{

const auto Hotkey_Compile = EditorHotkey{"RbScript.Compile"}.Ctrl().Press(KEY_B);
const auto Hotkey_Save = EditorHotkey{"RbScript.Save"}.Ctrl().Press(KEY_S);

ImVec4 TokenColor(RbScriptTokenKind kind)
{
    switch (kind)
    {
    case RbScriptTokenKind::Module:
    case RbScriptTokenKind::Use:
    case RbScriptTokenKind::Script:
    case RbScriptTokenKind::Fn:
    case RbScriptTokenKind::On:
    case RbScriptTokenKind::Async:
    case RbScriptTokenKind::Await:
    case RbScriptTokenKind::Return:
    case RbScriptTokenKind::If:
    case RbScriptTokenKind::Else:
    case RbScriptTokenKind::While:
    case RbScriptTokenKind::For:
    case RbScriptTokenKind::In:
    case RbScriptTokenKind::Let:
    case RbScriptTokenKind::Var:
    case RbScriptTokenKind::Const:
    case RbScriptTokenKind::Struct:
    case RbScriptTokenKind::Enum:
    case RbScriptTokenKind::Class:
    case RbScriptTokenKind::Signal:
    case RbScriptTokenKind::Emit:
    case RbScriptTokenKind::Match:
    case RbScriptTokenKind::Break:
    case RbScriptTokenKind::Continue:
    case RbScriptTokenKind::Public:
    case RbScriptTokenKind::Private:
    case RbScriptTokenKind::Static:
        return ImVec4{0.95f, 0.55f, 0.25f, 1.0f};
    case RbScriptTokenKind::True:
    case RbScriptTokenKind::False:
    case RbScriptTokenKind::Null:
        return ImVec4{0.70f, 0.75f, 1.0f, 1.0f};
    case RbScriptTokenKind::IntegerLiteral:
    case RbScriptTokenKind::FloatLiteral:
        return ImVec4{0.35f, 0.85f, 0.75f, 1.0f};
    case RbScriptTokenKind::StringLiteral:
        return ImVec4{0.45f, 0.90f, 0.45f, 1.0f};
    case RbScriptTokenKind::Identifier:
        return ImVec4{0.90f, 0.90f, 0.92f, 1.0f};
    default:
        return ImVec4{0.72f, 0.75f, 0.80f, 1.0f};
    }
}

bool IsAbsoluteFileName(const ea::string& path)
{
    return path.starts_with("/") || (path.size() > 1 && path[1] == ':');
}

ea::string ProjectFileName(Project* project, const ea::string& resourceName)
{
    if (!project || IsAbsoluteFileName(resourceName))
        return resourceName;
    return AddTrailingSlash(project->GetProjectPath()) + resourceName;
}

ea::string ReadTextFile(Context* context, const ea::string& fileName)
{
    File file(context, fileName, FILE_READ);
    if (!file.IsOpen())
        return {};
    const unsigned size = file.GetSize();
    ea::string source;
    source.resize(size);
    if (size && file.Read(&source[0], size) != size)
        return {};
    return source;
}

class RbScriptSourceSnapshotAction final : public EditorAction
{
public:
    RbScriptSourceSnapshotAction(RbScriptTab* tab, const ea::string& before, const ea::string& after)
        : tab_(tab)
        , before_(before)
        , after_(after)
    {
    }

    void Redo() const override
    {
        if (tab_)
            tab_->ApplySourceSnapshot(after_);
    }

    void Undo() const override
    {
        if (tab_)
            tab_->ApplySourceSnapshot(before_);
    }

private:
    WeakPtr<RbScriptTab> tab_;
    ea::string before_;
    ea::string after_;
};

}

void Foundation_RbScriptTab(Context* context, Project* project)
{
    project->AddTab(MakeShared<RbScriptTab>(context));
}

RbScriptTab::RbScriptTab(Context* context)
    : ResourceEditorTab(context, "rbscript", "4e4bd5ec-5085-4f88-89f8-2fbead3c7a9f",
          EditorTabFlag::None, EditorTabPlacement::DockCenter)
{
    BindHotkey(Hotkey_Compile, &RbScriptTab::CompileActiveDocument);
    BindHotkey(Hotkey_Save, &RbScriptTab::SaveCurrentResource);
    typeRegistry_.RegisterFromReflection(context);
}

void RbScriptTab::CreateNewScript()
{
    auto project = GetProject();
    auto fs = GetSubsystem<FileSystem>();
    if (!project || !fs)
        return;

    const ea::string scriptDirectory = AddTrailingSlash(project->GetDataPath()) + "Scripts";
    if (!fs->CreateDirsRecursive(scriptDirectory))
    {
        status_ = "Unable to create Data/Scripts";
        return;
    }

    ea::string resourceName = "Scripts/NewScript.rbscript";
    unsigned suffix = 1;
    while (fs->FileExists(AddTrailingSlash(project->GetDataPath()) + resourceName))
        resourceName = Format("Scripts/NewScript{}.rbscript", suffix++);

    const ea::string fileName = AddTrailingSlash(project->GetDataPath()) + resourceName;
    File file(context_, fileName, FILE_WRITE);
    if (!file.IsOpen())
    {
        status_ = Format("Unable to create {}", resourceName);
        return;
    }

    const ea::string source = RbScriptEditorContract::GetTemplateSource(templateIndex_);
    if (file.Write(source.data(), source.size()) != source.size())
    {
        status_ = Format("Unable to write {}", resourceName);
        return;
    }

    project->ProcessRequest(MakeShared<OpenResourceRequest>(context_, resourceName), this);
    status_ = Format("Created {}", resourceName);
}

void RbScriptTab::FocusResourceBrowser()
{
    if (auto project = GetProject())
    {
        if (auto browser = project->FindTab<ResourceBrowserTab>())
        {
            browser->Focus(true);
            status_ = "Select a .rbscript file in the Resources tab";
            return;
        }
    }
    status_ = "Resource Browser is unavailable";
}

bool RbScriptTab::CanOpenResource(const ResourceFileDescriptor& desc)
{
    return desc.HasObjectType<RbScriptResource>() || RbScriptEditorContract::IsRbScriptResourcePath(desc.resourceName_);
}

RbScriptTab::Document* RbScriptTab::GetActiveDocument()
{
    auto iter = documents_.find(GetActiveResourceName());
    return iter != documents_.end() ? &iter->second : nullptr;
}

const RbScriptTab::Document* RbScriptTab::GetActiveDocument() const
{
    auto iter = documents_.find(GetActiveResourceName());
    return iter != documents_.end() ? &iter->second : nullptr;
}

ea::string RbScriptTab::GetActiveSource() const
{
    return activeSource_;
}

void RbScriptTab::SetActiveSource(const ea::string& source)
{
    activeSource_ = source;
    if (Document* document = GetActiveDocument())
        document->source = source;
}

void RbScriptTab::ParseSymbols(Document& document, const ea::string& resourceName)
{
    document.module = RbScriptModule{};
    document.symbols.clear();
    RbScriptParser parser(document.tokens, resourceName);
    document.module = parser.ParseModule();
    for (const RbScriptDiagnostic& diagnostic : parser.GetDiagnostics())
        document.diagnostics.push_back(diagnostic);

    if (!document.module.name.empty())
        document.symbols.push_back({document.module.name, "module", {}});
    for (const RbScriptScript& script : document.module.scripts)
    {
        document.symbols.push_back({script.name, "script", script.span});
        for (const RbScriptField& field : script.fields)
            document.symbols.push_back({field.name, "field", field.span});
        for (const RbScriptFunction& function : script.functions)
            document.symbols.push_back({function.name, function.eventHandler ? "event" : "function", function.span});
    }
}

void RbScriptTab::TokenizeDocument(Document& document)
{
    RbScriptLexer lexer(document.source, GetActiveResourceName());
    document.tokens = lexer.Tokenize();
    document.diagnostics = lexer.GetDiagnostics();
    ParseSymbols(document, GetActiveResourceName());
}

void RbScriptTab::RefreshDocument(const ea::string& resourceName, bool compile)
{
    auto iter = documents_.find(resourceName);
    if (iter == documents_.end())
        return;

    Document& document = iter->second;
    const bool wasDebugging = document.debugVm.IsDebugging();
    const unsigned previousDebugLine = document.debugVm.GetCurrentLine();
    const ea::vector<unsigned> previousBreakpoints = document.debugVm.GetBreakpoints();
    document.debugVm.StopDebug();
    document.chunk = RbScriptChunk{};
    RbScriptLexer lexer(document.source, resourceName);
    document.tokens = lexer.Tokenize();
    document.diagnostics = lexer.GetDiagnostics();
    ParseSymbols(document, resourceName);
    document.compiled = false;
    document.debugStateMigrated = false;

    if (compile && document.diagnostics.empty())
    {
        RbScriptResource resource(context_);
        document.compiled = resource.CompileSource(document.source, resourceName);
        document.diagnostics = resource.GetDiagnostics();
        if (document.compiled)
            document.chunk = resource.GetChunk();
    }

    for (const unsigned line : previousBreakpoints)
        document.debugVm.SetBreakpoint(line);
    if (wasDebugging && document.compiled)
    {
        document.debugStateMigrated = document.debugVm.BeginDebug(document.chunk);
        for (const unsigned line : previousBreakpoints)
            document.debugVm.SetBreakpoint(line);
        if (previousDebugLine > 0)
            status_ = Format("rbscript hot reload migrated; previous line {}", previousDebugLine);
    }

    if (resourceName == GetActiveResourceName())
        activeSource_ = document.source;
}

void RbScriptTab::ApplySource(const ea::string& resourceName, const ea::string& source, bool compile)
{
    auto iter = documents_.find(resourceName);
    if (iter == documents_.end())
        return;
    iter->second.source = source;
    iter->second.dirty = true;
    RefreshDocument(resourceName, compile);
}

void RbScriptTab::LoadDocument(const ea::string& resourceName)
{
    Document& document = documents_[resourceName];
    document = Document{};

    auto cache = GetSubsystem<ResourceCache>();
    AbstractFilePtr file = cache ? cache->GetFile(resourceName, false) : nullptr;
    if (file && file->IsOpen())
    {
        const unsigned size = file->GetSize();
        document.source.resize(size);
        if (size && file->Read(&document.source[0], size) != size)
            document.source.clear();
    }

    RefreshDocument(resourceName, true);
    diskSources_[resourceName] = document.source;
    conflictPending_[resourceName] = false;
    activeSource_ = document.source;
    status_ = Format("Loaded {}", resourceName);
}

void RbScriptTab::SaveDocument(const ea::string& resourceName)
{
    auto iter = documents_.find(resourceName);
    if (iter == documents_.end())
        return;

    const ea::string fileName = ProjectFileName(GetProject(), resourceName);
    const ea::string source = iter->second.source;
    ignoreNextReload_ = true;
    File file(context_, fileName, FILE_WRITE);
    if (!file.IsOpen())
    {
        ignoreNextReload_ = false;
        status_ = Format("Unable to save {}", resourceName);
        return;
    }

    if (!source.empty() && file.Write(source.data(), source.size()) != source.size())
    {
        ignoreNextReload_ = false;
        status_ = Format("Unable to write {}", resourceName);
        return;
    }

    RefreshDocument(resourceName, true);
    iter->second.dirty = false;
    diskSources_[resourceName] = source;
    conflictPending_[resourceName] = false;
    status_ = Format("Saved {}", resourceName);
}

void RbScriptTab::PushSourceEdit(const ea::string& before, const ea::string& after)
{
    if (before == after || GetActiveResourceName().empty())
        return;
    PushAction<RbScriptSourceSnapshotAction>(this, before, after);
}

void RbScriptTab::ApplySourceSnapshot(const ea::string& source)
{
    if (GetActiveResourceName().empty())
        return;
    ApplySource(GetActiveResourceName(), source, autoCompile_);
    if (Document* document = GetActiveDocument())
        document->dirty = true;
    status_ = "rbscript edit restored";
}

bool RbScriptTab::WriteAutosaveSnapshot(const ea::string& directory, ea::vector<ea::string>& capturedFiles)
{
    if (!GetProject())
        return false;

    auto fileSystem = GetSubsystem<FileSystem>();
    bool capturedAny = false;
    for (const auto& [resourceName, document] : documents_)
    {
        if (!document.dirty || resourceName.empty() || resourceName.find("..") != ea::string::npos
            || resourceName.starts_with("/") || resourceName.find('\\') != ea::string::npos)
        {
            continue;
        }

        const ea::string destination = AddTrailingSlash(directory) + resourceName;
        if (!fileSystem->CreateDirsRecursive(GetPath(destination)))
            continue;

        File file(context_, destination, FILE_WRITE);
        if (!file.IsOpen())
            continue;
        if (!document.source.empty() && file.Write(document.source.data(), document.source.size()) != document.source.size())
            continue;

        capturedFiles.push_back(resourceName);
        capturedAny = true;
    }
    return capturedAny;
}

void RbScriptTab::CompileActiveDocument()
{
    if (GetActiveResourceName().empty())
        return;
    RefreshDocument(GetActiveResourceName(), true);
    const Document* document = GetActiveDocument();
    status_ = document && document->compiled ? "rbscript compiled successfully" : "rbscript compilation failed";
}

void RbScriptTab::RenderDiagnostics(const Document& document)
{
    if (!showDiagnostics_)
        return;

    ui::Separator();
    ui::Text("Diagnostics (%u)", static_cast<unsigned>(document.diagnostics.size()));
    for (unsigned i = 0; i < document.diagnostics.size(); ++i)
    {
        const RbScriptDiagnostic& diagnostic = document.diagnostics[i];
        const bool error = diagnostic.severity == RbScriptDiagnosticSeverity::Error;
        const ImVec4 color = error ? ImVec4{1.0f, 0.35f, 0.35f, 1.0f} : ImVec4{1.0f, 0.80f, 0.30f, 1.0f};
        ui::PushStyleColor(ImGuiCol_Text, color);
        ui::Text("%s:%u:%u [%s] %s", diagnostic.file.c_str(), diagnostic.span.begin.line,
            diagnostic.span.begin.column, diagnostic.code.c_str(), diagnostic.message.c_str());
        ui::PopStyleColor();
    }
}

void RbScriptTab::RenderDebugPanel(Document& document)
{
    ui::Separator();
    ui::Text("Debugger");
    if (!document.compiled)
    {
        ui::TextDisabled("Compile the document before starting a debug session.");
        return;
    }

    if (ui::Button("Start"))
    {
        if (document.debugVm.BeginDebug(document.chunk))
            status_ = "rbscript debug session started";
        else
            status_ = "rbscript debug session failed";
    }
    ui::SameLine();
    if (ui::Button("Step") && document.debugVm.IsDebugging())
    {
        document.debugVm.StepDebug();
        status_ = document.debugVm.HadError() ? "rbscript debug step failed" : "rbscript debug step complete";
    }
    ui::SameLine();
    if (ui::Button("Step Over") && document.debugVm.IsDebugging())
        StepOverDebug(document);
    ui::SameLine();
    if (ui::Button("Continue") && document.debugVm.IsDebugging())
    {
        document.debugVm.ContinueDebug();
        status_ = document.debugVm.HadError() ? "rbscript debug continue failed" : "rbscript debug continued";
    }
    ui::SameLine();
    if (ui::Button("Stop") && document.debugVm.IsDebugging())
    {
        document.debugVm.StopDebug();
        status_ = "rbscript debug session stopped";
    }

    int line = static_cast<int>(breakpointLine_);
    ui::SetNextItemWidth(100.0f);
    if (ui::InputInt("Breakpoint line", &line, 1, 10))
        breakpointLine_ = line > 0 ? static_cast<unsigned>(line) : 1;
    ui::SameLine();
    if (ui::SmallButton("Add breakpoint"))
        document.debugVm.SetBreakpoint(breakpointLine_);
    ui::SameLine();
    if (ui::SmallButton("Remove breakpoint"))
        document.debugVm.RemoveBreakpoint(breakpointLine_);

    ui::Separator();
    ui::Text("Breakpoints");
    const ea::vector<unsigned> activeBreakpoints = document.debugVm.GetBreakpoints();
    for (const unsigned activeLine : activeBreakpoints)
    {
        ui::PushID(static_cast<int>(activeLine));
        ui::BulletText("line %u", activeLine);
        ui::SameLine();
        if (ui::SmallButton("Remove"))
            document.debugVm.RemoveBreakpoint(activeLine);
        ui::PopID();
    }

    ui::Separator();
    ui::Text("Watches");
    ui::SetNextItemWidth(250.0f);
    ui::InputText("Expression", &watchInput_);
    ui::SameLine();
    if (ui::SmallButton("Add watch") && !watchInput_.empty())
    {
        bool exists = false;
        for (const ea::string& expression : document.watchExpressions)
            exists = exists || expression == watchInput_;
        if (!exists)
            document.watchExpressions.push_back(watchInput_);
        watchInput_.clear();
    }
    for (unsigned i = 0; i < document.watchExpressions.size(); ++i)
    {
        const ea::string& expression = document.watchExpressions[i];
        ui::PushID(static_cast<int>(i));
        const auto local = document.debugVm.GetLocals().find(expression);
        ui::BulletText("%s = %s", expression.c_str(),
            local != document.debugVm.GetLocals().end() ? local->second.ToString().c_str() : "<not a local>");
        ui::SameLine();
        if (ui::SmallButton("Remove"))
        {
            document.watchExpressions.erase(document.watchExpressions.begin() + i);
            ui::PopID();
            break;
        }
        ui::PopID();
    }

    if (document.debugVm.IsDebugging())
    {
        ui::Text("State: %s | line: %u", document.debugVm.IsDebugPaused() ? "paused" : "running",
            document.debugVm.GetCurrentLine());
        if (ui::BeginChild("##RbScriptDebugState", ImVec2{0.0f, 145.0f}, true))
        {
            ui::Text("Call stack");
            for (const ea::string& frame : document.debugVm.GetCallStack())
                ui::BulletText("%s", frame.c_str());
            ui::Separator();
            ui::Text("Locals");
            for (const auto& local : document.debugVm.GetLocals())
                ui::Text("%s = %s", local.first.c_str(), local.second.ToString().c_str());
            ui::EndChild();
        }
    }
}

void RbScriptTab::RenderTokenPreview(const Document& document)
{
    if (!showPreview_)
        return;

    ui::BeginChild("##RbScriptPreview", ImVec2{0.0f, 190.0f}, true);
    ui::Text("Lexical preview");
    ui::Separator();

    unsigned offset = 0;
    for (const RbScriptToken& token : document.tokens)
    {
        if (token.kind == RbScriptTokenKind::EndOfFile)
            break;

        if (token.span.begin.offset > offset)
        {
            const ea::string whitespace = document.source.substr(offset, token.span.begin.offset - offset);
            ui::TextUnformatted(whitespace.c_str());
            if (!whitespace.empty() && whitespace.back() != '\n')
                ui::SameLine(0.0f, 0.0f);
        }

        ui::TextColored(TokenColor(token.kind), "%s", token.lexeme.c_str());
        if (token.lexeme.find('\n') == ea::string::npos)
            ui::SameLine(0.0f, 0.0f);
        offset = token.span.end.offset;
    }
    ui::EndChild();
}

void RbScriptTab::RenderAutocomplete(const Document& document)
{
    if (!ui::CollapsingHeader("Reflection autocomplete", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    ui::InputText("Completion filter", &searchText_);
    if (searchText_.empty())
        return;

    ea::vector<ea::string> suggestions = {
        "module", "use", "script", "fn", "on", "async", "let", "var", "const",
        "if", "else", "while", "return", "emit", "await", "match", "break", "continue",
        "Vector2", "Vector3", "Quaternion", "Color", "Node", "Component", "Resource", "Variant",
        "Array", "Map", "Optional",
    };
    for (const ea::string& name : typeRegistry_.GetTypeNames())
        suggestions.push_back(name);
    for (const ea::string& name : typeRegistry_.GetFunctionNames())
        suggestions.push_back(name);

    ui::Text("Suggestions from rbfx reflection and rbscript keywords");
    unsigned shown = 0;
    for (const ea::string& suggestion : suggestions)
    {
        if (suggestion.find(searchText_) == ea::string::npos)
            continue;
        if (shown++ >= 48)
            break;
        ui::SameLine();
        if (ui::SmallButton(suggestion.c_str()))
            status_ = Format("Suggestion: {}", suggestion);
    }
}

void RbScriptTab::RenderOutline(Document& document)
{
    if (!showOutline_ || !ui::CollapsingHeader("Symbol outline", ImGuiTreeNodeFlags_DefaultOpen))
        return;
    if (document.symbols.empty())
    {
        ui::TextDisabled("No symbols available; fix parser diagnostics first.");
        return;
    }

    for (unsigned i = 0; i < document.symbols.size(); ++i)
    {
        const Document::Symbol& symbol = document.symbols[i];
        ui::PushID(static_cast<int>(i));
        const bool selected = selectedSymbol_ == static_cast<int>(i);
        if (ui::Selectable(Format("{}  {}", symbol.kind, symbol.name).c_str(), selected))
        {
            selectedSymbol_ = static_cast<int>(i);
            status_ = Format("{} '{}' at line {}, column {}", symbol.kind, symbol.name,
                symbol.span.begin.line, symbol.span.begin.column);
        }
        if (ui::IsItemHovered())
            ui::SetTooltip("Go to definition: line %u, column %u", symbol.span.begin.line, symbol.span.begin.column);
        ui::PopID();
    }

    if (selectedSymbol_ >= 0 && selectedSymbol_ < static_cast<int>(document.symbols.size()))
    {
        const Document::Symbol& symbol = document.symbols[selectedSymbol_];
        ui::InputText("Rename to", &renameText_);
        ui::SameLine();
        if (ui::SmallButton("Rename") && !renameText_.empty())
        {
            RenameSymbol(document, symbol, renameText_);
            renameText_.clear();
        }
    }
}

void RbScriptTab::ReplaceAll(Document& document, const ea::string& find, const ea::string& replacement)
{
    if (find.empty())
        return;
    const ea::string before = document.source;
    ea::string updated;
    ea::string::size_type cursor = 0;
    unsigned count = 0;
    while (cursor < before.size())
    {
        const ea::string::size_type match = before.find(find, cursor);
        if (match == ea::string::npos)
        {
            updated += before.substr(cursor);
            break;
        }
        updated += before.substr(cursor, match - cursor);
        updated += replacement;
        cursor = match + find.size();
        ++count;
    }
    if (!count)
        return;

    PushSourceEdit(before, updated);
    document.source = updated;
    document.dirty = true;
    activeSource_ = updated;
    RefreshDocument(GetActiveResourceName(), autoCompile_);
    status_ = Format("Replaced {} occurrence(s)", count);
}

void RbScriptTab::RenameSymbol(Document& document, const Document::Symbol& symbol, const ea::string& replacement)
{
    ReplaceAll(document, symbol.name, replacement);
}

void RbScriptTab::RenderFindReplace(Document& document)
{
    if (!showFindReplace_)
        return;
    if (!ui::BeginChild("##RbScriptFindReplace", ImVec2{0.0f, 86.0f}, true))
    {
        ui::EndChild();
        return;
    }
    ui::Text("Find and replace");
    ui::SetNextItemWidth(220.0f);
    ui::InputText("Find", &findText_);
    ui::SameLine();
    ui::SetNextItemWidth(220.0f);
    ui::InputText("Replace", &replaceText_);
    ui::SameLine();
    if (ui::Button("Replace all"))
        ReplaceAll(document, findText_, replaceText_);
    ui::EndChild();
}

void RbScriptTab::StepOverDebug(Document& document)
{
    if (!document.debugVm.IsDebugging())
        return;
    const unsigned initialDepth = document.debugVm.GetCallStack().size();
    unsigned guard = 0;
    do
    {
        if (!document.debugVm.StepDebug())
            break;
        if (++guard > 100000)
            break;
    } while (document.debugVm.IsDebugging() && document.debugVm.GetCallStack().size() > initialDepth);
    status_ = document.debugVm.HadError() ? "rbscript step-over failed" : "rbscript step-over complete";
}

void RbScriptTab::RenderConflictDialog()
{
    if (conflictDialogPending_)
    {
        ui::OpenPopup("rbscript Conflict");
        conflictDialogPending_ = false;
    }
    if (!ui::BeginPopupModal("rbscript Conflict", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;

    ui::TextWrapped("The resource '%s' changed on disk while it has unsaved editor changes.", conflictResource_.c_str());
    if (ui::Button("Keep mine", ImVec2{120.0f, 0.0f}))
    {
        conflictPending_[conflictResource_] = false;
        diskSources_[conflictResource_] = conflictDiskSource_;
        status_ = Format("Kept editor changes for {}", conflictResource_);
        ui::CloseCurrentPopup();
    }
    ui::SameLine();
    if (ui::Button("Use disk", ImVec2{120.0f, 0.0f}))
    {
        ApplySource(conflictResource_, conflictDiskSource_, autoCompile_);
        auto iter = documents_.find(conflictResource_);
        if (iter != documents_.end())
            iter->second.dirty = false;
        diskSources_[conflictResource_] = conflictDiskSource_;
        conflictPending_[conflictResource_] = false;
        status_ = Format("Loaded disk version of {}", conflictResource_);
        ui::CloseCurrentPopup();
    }
    ui::SameLine();
    if (ui::Button(conflictShowDiff_ ? "Hide diff" : "Show diff"))
        conflictShowDiff_ = !conflictShowDiff_;
    if (conflictShowDiff_)
    {
        const Document* document = documents_.find(conflictResource_) != documents_.end()
            ? &documents_.find(conflictResource_)->second : nullptr;
        ui::Separator();
        ui::Text("Editor bytes: %u | Disk bytes: %u", document ? document->source.size() : 0, conflictDiskSource_.size());
        if (document)
        {
            const unsigned common = Min(document->source.size(), conflictDiskSource_.size());
            unsigned firstDifference = 0;
            while (firstDifference < common && document->source[firstDifference] == conflictDiskSource_[firstDifference])
                ++firstDifference;
            ui::Text("First difference at byte %u", firstDifference);
        }
    }
    ui::EndPopup();
}

void RbScriptTab::RenderContent()
{
    RenderConflictDialog();
    Document* document = GetActiveDocument();
    if (!document)
    {
        ui::TextWrapped("Create or open a .rbscript resource to start editing.");
        if (ui::Button("New rbscript"))
            CreateNewScript();
        ui::SameLine();
        if (ui::Button("Open in Resource Browser"))
            FocusResourceBrowser();
        if (!status_.empty())
            ui::Text("%s", status_.c_str());
        return;
    }

    const ea::string before = activeSource_;
    const ImVec2 editorSize = ImVec2{0.0f, showPreview_ ? -210.0f : -150.0f};
    ImFont* monoFont = Project::GetMonoFont();
    if (monoFont)
        ui::PushFont(monoFont);

    const bool changed = ui::InputTextMultiline("##RbScriptSource", &activeSource_, editorSize,
        ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_CallbackAlways);
    sourceFocused_ = ui::IsItemActive();
    if (monoFont)
        ui::PopFont();

    if (changed)
    {
        document->source = activeSource_;
        document->dirty = true;
        RefreshDocument(GetActiveResourceName(), autoCompile_);
        PushSourceEdit(before, activeSource_);
    }

    RenderFindReplace(*document);
    RenderOutline(*document);
    RenderAutocomplete(*document);
    RenderTokenPreview(*document);
    RenderDiagnostics(*document);
    RenderDebugPanel(*document);
}

void RbScriptTab::RenderToolbar()
{
    static const char* templateNames[] = {"Empty", "Component", "Gameplay", "Network"};
    ui::SetNextItemWidth(120.0f);
    if (ui::BeginCombo("Template", templateNames[templateIndex_]))
    {
        for (unsigned i = 0; i < 4; ++i)
        {
            if (ui::Selectable(templateNames[i], i == templateIndex_))
                templateIndex_ = i;
        }
        ui::EndCombo();
    }
    ui::SameLine();
    if (ui::Button("New rbscript"))
        CreateNewScript();
    ui::SameLine();
    if (ui::Button("Open Browser"))
        FocusResourceBrowser();
    ui::SameLine();
    if (ui::Button("Compile"))
        CompileActiveDocument();
    ui::SameLine();
    if (ui::Button("Save"))
        SaveCurrentResource();
    ui::SameLine();
    ui::Checkbox("Auto compile", &autoCompile_);
    ui::SameLine();
    ui::Checkbox("Preview", &showPreview_);
    ui::SameLine();
    ui::Checkbox("Diagnostics", &showDiagnostics_);
    ui::SameLine();
    if (ui::Button(showOutline_ ? "Hide outline" : "Show outline"))
        showOutline_ = !showOutline_;
    ui::SameLine();
    if (ui::Button("Find/Replace"))
        showFindReplace_ = !showFindReplace_;
    ui::SameLine();
    ui::Text("%s", status_.c_str());
}

void RbScriptTab::RenderContextMenuItems()
{
    ResourceEditorTab::RenderContextMenuItems();
    contextMenuSeparator_.Reset();
    if (ui::MenuItem("Compile rbscript", GetHotkeyLabel(Hotkey_Compile).c_str()))
        CompileActiveDocument();
    ui::MenuItem("Lexical preview", nullptr, &showPreview_);
    ui::MenuItem("Diagnostics", nullptr, &showDiagnostics_);
    ui::MenuItem("Symbol outline", nullptr, &showOutline_);
    ui::MenuItem("Find/replace", nullptr, &showFindReplace_);
}

void RbScriptTab::WriteIniSettings(ImGuiTextBuffer& output)
{
    ResourceEditorTab::WriteIniSettings(output);
    WriteStringToIni(output, "AutoCompile", Format("{}", autoCompile_ ? 1 : 0));
    WriteStringToIni(output, "Preview", Format("{}", showPreview_ ? 1 : 0));
    WriteStringToIni(output, "Diagnostics", Format("{}", showDiagnostics_ ? 1 : 0));
    WriteStringToIni(output, "Outline", Format("{}", showOutline_ ? 1 : 0));
    WriteStringToIni(output, "FindReplace", Format("{}", showFindReplace_ ? 1 : 0));
    WriteStringToIni(output, "Template", Format("{}", templateIndex_));
}

void RbScriptTab::ReadIniSettings(const char* line)
{
    ResourceEditorTab::ReadIniSettings(line);
    if (const auto value = ReadStringFromIni(line, "AutoCompile"))
        autoCompile_ = ToInt(*value) != 0;
    if (const auto value = ReadStringFromIni(line, "Preview"))
        showPreview_ = ToInt(*value) != 0;
    if (const auto value = ReadStringFromIni(line, "Diagnostics"))
        showDiagnostics_ = ToInt(*value) != 0;
    if (const auto value = ReadStringFromIni(line, "Outline"))
        showOutline_ = ToInt(*value) != 0;
    if (const auto value = ReadStringFromIni(line, "FindReplace"))
        showFindReplace_ = ToInt(*value) != 0;
    if (const auto value = ReadStringFromIni(line, "Template"))
    {
        const int parsedTemplate = ToInt(*value);
        templateIndex_ = static_cast<unsigned>(parsedTemplate < 0 ? 0 : parsedTemplate > 3 ? 3 : parsedTemplate);
    }
}

void RbScriptTab::OnResourceLoaded(const ea::string& resourceName)
{
    const ea::string diskSource = ReadTextFile(context_, ProjectFileName(GetProject(), resourceName));
    auto iter = documents_.find(resourceName);
    if (ignoreNextReload_)
    {
        ignoreNextReload_ = false;
        diskSources_[resourceName] = diskSource;
        conflictPending_[resourceName] = false;
        return;
    }

    if (iter != documents_.end() && iter->second.dirty && diskSource != iter->second.source
        && diskSource != diskSources_[resourceName])
    {
        conflictResource_ = resourceName;
        conflictDiskSource_ = diskSource;
        conflictPending_[resourceName] = true;
        conflictDialogPending_ = resourceName == GetActiveResourceName();
        status_ = Format("External rbscript change detected in {}", resourceName);
        return;
    }

    LoadDocument(resourceName);
}

void RbScriptTab::OnResourceUnloaded(const ea::string& resourceName)
{
    documents_.erase(resourceName);
    diskSources_.erase(resourceName);
    conflictPending_.erase(resourceName);
    if (resourceName == GetActiveResourceName())
        activeSource_.clear();
}

void RbScriptTab::OnActiveResourceChanged(const ea::string& oldResourceName, const ea::string& newResourceName)
{
    (void)oldResourceName;
    auto iter = documents_.find(newResourceName);
    activeSource_ = iter != documents_.end() ? iter->second.source : ea::string{};
    status_ = newResourceName.empty() ? ea::string{} : Format("Active: {}", newResourceName);
}

void RbScriptTab::OnResourceSaved(const ea::string& resourceName)
{
    SaveDocument(resourceName);
}

void RbScriptTab::OnResourceShallowSaved(const ea::string& resourceName)
{
    (void)resourceName;
}

} // namespace Urho3D
