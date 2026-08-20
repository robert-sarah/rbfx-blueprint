// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include "../Core/StringUtils.h"

#include <set>
#include <string>
#include <vector>

namespace Urho3D
{

enum class ProductionSessionRole
{
    Client,
    ListenServer,
    DedicatedServer
};

struct URHO3D_API ProductionOnlineSessionSettings
{
    std::string sessionId;
    ProductionSessionRole role{ProductionSessionRole::Client};
    unsigned maxPlayers{2};
    unsigned tickRate{60};
    unsigned inputDelayFrames{2};
    unsigned rollbackWindowFrames{120};
    bool requireHandshake{true};
    bool encryptedTransport{true};
    bool enableAntiCheatHooks{true};
};

struct URHO3D_API ProductionOnlineSessionValidation
{
    bool valid{};
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

/// Deterministic session admission contract for a transport or online backend.
///
/// It owns peer admission and capacity rules, but deliberately does not claim
/// to implement matchmaking, encryption or anti-cheat attestation. Those are
/// supplied by platform adapters through the explicit policy flags.
class URHO3D_API ProductionOnlineSession
{
public:
    bool Configure(const ProductionOnlineSessionSettings& settings, std::string* error = nullptr);
    static ProductionOnlineSessionValidation Validate(const ProductionOnlineSessionSettings& settings);

    bool AddPeer(const std::string& peerId, bool handshakeAuthenticated);
    bool RemovePeer(const std::string& peerId);
    bool HasPeer(const std::string& peerId) const;
    void ClearPeers();

    const ProductionOnlineSessionSettings& GetSettings() const { return settings_; }
    unsigned GetPeerCount() const { return static_cast<unsigned>(peers_.size()); }
    bool IsConfigured() const { return configured_; }

private:
    ProductionOnlineSessionSettings settings_;
    std::set<std::string> peers_;
    bool configured_{};
};

} // namespace Urho3D
