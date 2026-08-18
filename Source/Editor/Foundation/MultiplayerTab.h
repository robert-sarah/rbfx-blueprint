#pragma once

#include "../Project/ResourceEditorTab.h"

#include <Urho3D/Network/MultiplayerProfileResource.h>

namespace Urho3D
{

void Foundation_MultiplayerTab(Context* context, Project* project);

/// Production editor for persisted multiplayer startup and replication profiles.
class MultiplayerTab : public ResourceEditorTab
{
    URHO3D_OBJECT(MultiplayerTab, ResourceEditorTab);

public:
    explicit MultiplayerTab(Context* context);

    void RenderContent() override;
    void RenderToolbar() override;
    void RenderContextMenuItems() override;
    bool CanOpenResource(const ResourceFileDescriptor& desc) override;
    bool SupportMultipleResources() override { return false; }
    ea::string GetResourceTitle() override { return "Multiplayer Profile"; }
    bool IsUndoSupported() override { return true; }

    void ApplyProfileSnapshot(const JSONValue& snapshot);

protected:
    void OnResourceLoaded(const ea::string& resourceName) override;
    void OnResourceUnloaded(const ea::string& resourceName) override;
    void OnActiveResourceChanged(const ea::string& oldResourceName, const ea::string& newResourceName) override;
    void OnResourceSaved(const ea::string& resourceName) override;
    void OnResourceShallowSaved(const ea::string& resourceName) override;

private:
    MultiplayerProfileResource& GetProfile();
    const MultiplayerProfileResource& GetProfile() const;
    JSONValue CaptureProfile() const;
    void CommitProfileEdit(const JSONValue& before, const ea::string& status);
    void ResetTemplate();
    void ValidateProfile();
    void ApplyToLiveNetwork();
    void RenderStartupSettings(MultiplayerProfileResource& profile);
    void RenderReplicationSettings(MultiplayerProfileResource& profile);
    void RenderLiveDiagnostics();

    SharedPtr<MultiplayerProfileResource> resource_;
    MultiplayerProfileResource preview_;
    ea::string status_;
    ea::string validationError_;
    ea::string liveError_;
};

} // namespace Urho3D
