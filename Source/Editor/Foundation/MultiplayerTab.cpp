#include "MultiplayerTab.h"

#include "../Core/EditorIcons.h"
#include "../Core/EditorTheme.h"
#include "../Project/Project.h"

#include <Urho3D/Core/StringUtils.h>
#include <Urho3D/Network/Network.h>
#include <Urho3D/Resource/ResourceCache.h>
#include <Urho3D/SystemUI/SystemUI.h>

namespace Urho3D
{

namespace
{

class MultiplayerSnapshotAction final : public EditorAction
{
public:
    MultiplayerSnapshotAction(MultiplayerTab* tab, const JSONValue& before, const JSONValue& after)
        : tab_(tab)
        , before_(before)
        , after_(after)
    {
    }

    void Redo() const override
    {
        if (tab_)
            tab_->ApplyProfileSnapshot(after_);
    }

    void Undo() const override
    {
        if (tab_)
            tab_->ApplyProfileSnapshot(before_);
    }

private:
    WeakPtr<MultiplayerTab> tab_;
    JSONValue before_;
    JSONValue after_;
};

} // namespace

void Foundation_MultiplayerTab(Context* context, Project* project)
{
    project->AddTab(MakeShared<MultiplayerTab>(context));
}

MultiplayerTab::MultiplayerTab(Context* context)
    : ResourceEditorTab(context, "Multiplayer Profile", "6c4c5a92-6a2f-4b2f-9d8d-3a6b64e95e3d",
          EditorTabFlag::OpenByDefault, EditorTabPlacement::DockCenter)
    , preview_(context)
{
    ResetTemplate();
}

bool MultiplayerTab::CanOpenResource(const ResourceFileDescriptor& desc)
{
    return desc.HasObjectType<MultiplayerProfileResource>();
}

MultiplayerProfileResource& MultiplayerTab::GetProfile()
{
    return resource_ ? *resource_ : preview_;
}

const MultiplayerProfileResource& MultiplayerTab::GetProfile() const
{
    return resource_ ? *resource_ : preview_;
}

JSONValue MultiplayerTab::CaptureProfile() const
{
    return GetProfile().ToJSON();
}

void MultiplayerTab::ApplyProfileSnapshot(const JSONValue& snapshot)
{
    ea::string error;
    if (!GetProfile().FromJSON(snapshot, &error))
    {
        validationError_ = error;
        return;
    }
    validationError_.clear();
    status_ = "Multiplayer profile restored";
}

void MultiplayerTab::CommitProfileEdit(const JSONValue& before, const ea::string& status)
{
    const JSONValue after = CaptureProfile();
    if (before == after)
        return;
    GetUndoManager()->PushAction(MakeShared<MultiplayerSnapshotAction>(this, before, after));
    status_ = status;
    ValidateProfile();
}

void MultiplayerTab::ResetTemplate()
{
    preview_.SetReplicationSettings(StringVariantMap{});
    preview_.SetMode(MultiplayerProfileResource::Mode::ListenServer);
    preview_.SetAddress("127.0.0.1");
    preview_.SetPort(2345);
    preview_.SetMaxConnections(128);
    preview_.SetUpdateFps(30);
    preview_.SetPingIntervalMs(250);
    preview_.SetMaxPingIntervalMs(10000);
    preview_.SetClockBufferSize(40);
    preview_.SetPingBufferSize(10);
    preview_.SetPackageCacheDir("Cache/Packages");
    status_ = "Multiplayer profile template ready";
    validationError_.clear();
    liveError_.clear();
}

void MultiplayerTab::ValidateProfile()
{
    ea::string error;
    if (GetProfile().Validate(&error))
    {
        validationError_.clear();
        status_ = Format("Profile valid: digest {}", GetProfile().ComputeDigest());
    }
    else
    {
        validationError_ = error;
        status_ = "Multiplayer profile validation failed";
    }
}

void MultiplayerTab::ApplyToLiveNetwork()
{
    ea::string error;
    Network* network = GetSubsystem<Network>();
    if (GetProfile().ApplyToNetwork(network, &error))
    {
        liveError_.clear();
        status_ = "Network tuning applied; no connection was started";
    }
    else
        liveError_ = error;
}

void MultiplayerTab::RenderToolbar()
{
    EditorTheme::PushToolbarColors();
    if (ui::Button(EditorIcons::ResetLabel))
        ResetTemplate();
    ui::SameLine();
    if (ui::Button(EditorIcons::ValidateLabel))
        ValidateProfile();
    ui::SameLine();
    if (ui::Button(ICON_FA_NETWORK_WIRED " Apply Network Tuning"))
        ApplyToLiveNetwork();
    EditorTheme::PopToolbarColors();
    ui::SameLine();
    ui::TextColored(EditorThemeColors::ToColor(EditorThemeColors::TextMuted), "%s", status_.c_str());
}

void MultiplayerTab::RenderStartupSettings(MultiplayerProfileResource& profile)
{
    ui::Text("Startup and transport profile");
    const JSONValue before = CaptureProfile();
    static const char* modes[] = {"Server", "Client", "ListenServer"};
    int mode = static_cast<int>(profile.GetMode());
    if (ui::Combo("Mode", &mode, modes, IM_ARRAYSIZE(modes)))
    {
        profile.SetMode(static_cast<MultiplayerProfileResource::Mode>(mode));
        CommitProfileEdit(before, "Changed multiplayer mode");
    }

    ea::string address = profile.GetAddress();
    if (ui::InputText("Address", &address))
    {
        profile.SetAddress(address);
        CommitProfileEdit(before, "Changed server address");
    }

    int port = static_cast<int>(profile.GetPort());
    if (ui::InputInt("Port", &port))
    {
        profile.SetPort(static_cast<unsigned>(ea::max(0, port)));
        CommitProfileEdit(before, "Changed server port");
    }

    int maxConnections = static_cast<int>(profile.GetMaxConnections());
    if (ui::InputInt("Max connections", &maxConnections))
    {
        profile.SetMaxConnections(static_cast<unsigned>(ea::max(0, maxConnections)));
        CommitProfileEdit(before, "Changed connection capacity");
    }

    int updateFps = static_cast<int>(profile.GetUpdateFps());
    if (ui::InputInt("Network update FPS", &updateFps))
    {
        profile.SetUpdateFps(static_cast<unsigned>(ea::max(0, updateFps)));
        CommitProfileEdit(before, "Changed network update frequency");
    }

    int pingInterval = static_cast<int>(profile.GetPingIntervalMs());
    if (ui::InputInt("Ping interval (ms)", &pingInterval))
    {
        profile.SetPingIntervalMs(static_cast<unsigned>(ea::max(0, pingInterval)));
        CommitProfileEdit(before, "Changed ping interval");
    }

    int maxPing = static_cast<int>(profile.GetMaxPingIntervalMs());
    if (ui::InputInt("Max ping (ms)", &maxPing))
    {
        profile.SetMaxPingIntervalMs(static_cast<unsigned>(ea::max(0, maxPing)));
        CommitProfileEdit(before, "Changed max ping");
    }

    int clockBuffer = static_cast<int>(profile.GetClockBufferSize());
    if (ui::InputInt("Clock samples", &clockBuffer))
    {
        profile.SetClockBufferSize(static_cast<unsigned>(ea::max(0, clockBuffer)));
        CommitProfileEdit(before, "Changed clock buffer");
    }

    int pingBuffer = static_cast<int>(profile.GetPingBufferSize());
    if (ui::InputInt("Ping samples", &pingBuffer))
    {
        profile.SetPingBufferSize(static_cast<unsigned>(ea::max(0, pingBuffer)));
        CommitProfileEdit(before, "Changed ping buffer");
    }

    ea::string packageCacheDir = profile.GetPackageCacheDir();
    if (ui::InputText("Package cache directory", &packageCacheDir))
    {
        profile.SetPackageCacheDir(packageCacheDir);
        CommitProfileEdit(before, "Changed package cache directory");
    }
}

void MultiplayerTab::RenderReplicationSettings(MultiplayerProfileResource& profile)
{
    ui::Separator();
    ui::Text("Replication settings");

    const auto editFloat = [&](const char* label, const char* key, float minimum, float maximum)
    {
        auto iter = profile.GetReplicationSettings().find(key);
        if (iter == profile.GetReplicationSettings().end())
            return;
        const JSONValue before = CaptureProfile();
        float value = iter->second.GetFloat();
        if (ui::InputFloat(label, &value, 0.01f, 0.1f, "%.3f"))
        {
            iter->second = ea::max(minimum, ea::min(maximum, value));
            CommitProfileEdit(before, Format("Changed {}", key));
        }
    };

    const auto editUInt = [&](const char* label, const char* key, unsigned minimum, unsigned maximum)
    {
        auto iter = profile.GetReplicationSettings().find(key);
        if (iter == profile.GetReplicationSettings().end())
            return;
        const JSONValue before = CaptureProfile();
        int value = static_cast<int>(iter->second.GetUInt());
        if (ui::InputInt(label, &value))
        {
            value = ea::max(static_cast<int>(minimum), ea::min(static_cast<int>(maximum), value));
            iter->second = static_cast<unsigned>(value);
            CommitProfileEdit(before, Format("Changed {}", key));
        }
    };

    editFloat("Interpolation limit", "InterpolationLimit", 0.0f, 10.0f);
    editUInt("Max input frames", "MaxInputFrames", 1, 4096);
    editUInt("Max input redundancy", "MaxInputRedundancy", 1, 256);
    editFloat("Interpolation delay", "InterpolationDelay", 0.0f, 10.0f);
    editFloat("Min time dilation", "MinTimeDilation", 0.01f, 4.0f);
    editFloat("Max time dilation", "MaxTimeDilation", 0.01f, 4.0f);
    editFloat("Relevance timeout", "RelevanceTimeout", 0.0f, 120.0f);
    editFloat("Server tracing duration", "ServerTracingDuration", 0.0f, 120.0f);
    editFloat("Client tracing duration", "ClientTracingDuration", 0.0f, 120.0f);
}

void MultiplayerTab::RenderLiveDiagnostics()
{
    Network* network = GetSubsystem<Network>();
    if (!network)
    {
        ui::TextUnformatted("Network subsystem unavailable");
        return;
    }

    ui::Separator();
    ui::Text("Live diagnostics");
    ui::Text("Server running: %s", network->IsServerRunning() ? "yes" : "no");
    ui::Text("Client connections: %u", static_cast<unsigned>(network->GetClientConnections().size()));
    if (network->GetServerConnection())
        ui::TextUnformatted("Connected to server: yes");
    else
        ui::TextUnformatted("Connected to server: no");
    const ea::string debugInfo = network->GetDebugInfo();
    if (!debugInfo.empty())
        ui::TextWrapped("%s", debugInfo.c_str());
    if (!liveError_.empty())
    {
        EditorTheme::PushDiagnosticText(true);
        ui::Text("Apply error: %s", liveError_.c_str());
        EditorTheme::PopDiagnosticText();
    }
}

void MultiplayerTab::RenderContent()
{
    MultiplayerProfileResource& profile = GetProfile();
    if (ui::BeginTable("MultiplayerProfileLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
    {
        ui::TableSetupColumn("Profile", ImGuiTableColumnFlags_WidthFixed, 420.0f);
        ui::TableSetupColumn("Diagnostics", ImGuiTableColumnFlags_WidthStretch);
        ui::TableNextRow();
        ui::TableSetColumnIndex(0);
        RenderStartupSettings(profile);
        RenderReplicationSettings(profile);
        ui::TableSetColumnIndex(1);
        RenderLiveDiagnostics();
        ui::EndTable();
    }

    if (!validationError_.empty())
    {
        EditorTheme::PushDiagnosticText(true);
        ui::Text("Validation error: %s", validationError_.c_str());
        EditorTheme::PopDiagnosticText();
    }
}

void MultiplayerTab::RenderContextMenuItems()
{
    if (ui::MenuItem("Reset multiplayer profile template"))
        ResetTemplate();
    if (ui::MenuItem("Validate multiplayer profile"))
        ValidateProfile();
}

void MultiplayerTab::OnResourceLoaded(const ea::string& resourceName)
{
    resource_ = GetSubsystem<ResourceCache>()->GetResource<MultiplayerProfileResource>(resourceName);
    validationError_.clear();
    liveError_.clear();
    status_ = resource_ ? "Multiplayer profile loaded" : "Unable to load multiplayer profile";
}

void MultiplayerTab::OnResourceUnloaded(const ea::string& resourceName)
{
    if (resourceName == GetActiveResourceName())
        resource_.Reset();
}

void MultiplayerTab::OnActiveResourceChanged(const ea::string&, const ea::string& newResourceName)
{
    if (newResourceName.empty())
    {
        resource_.Reset();
        return;
    }
    resource_ = GetSubsystem<ResourceCache>()->GetResource<MultiplayerProfileResource>(newResourceName);
    validationError_.clear();
    liveError_.clear();
}

void MultiplayerTab::OnResourceSaved(const ea::string& resourceName)
{
    status_ = Format("Saved {}", resourceName);
}

void MultiplayerTab::OnResourceShallowSaved(const ea::string&)
{
}

} // namespace Urho3D
