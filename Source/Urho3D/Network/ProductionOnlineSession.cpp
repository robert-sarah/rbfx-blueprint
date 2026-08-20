// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#include "ProductionOnlineSession.h"

namespace Urho3D
{

namespace
{
void SetError(std::string* error, const char* message)
{
    if (error)
        *error = message;
}
}

ProductionOnlineSessionValidation ProductionOnlineSession::Validate(const ProductionOnlineSessionSettings& settings)
{
    ProductionOnlineSessionValidation result;
    result.valid = true;

    if (settings.sessionId.empty())
        result.errors.emplace_back("sessionId must not be empty");
    if (settings.maxPlayers == 0 || settings.maxPlayers > 256)
        result.errors.emplace_back("maxPlayers must be within [1, 256]");
    if (settings.tickRate == 0 || settings.tickRate > 240)
        result.errors.emplace_back("tickRate must be within [1, 240]");
    if (settings.rollbackWindowFrames == 0 || settings.rollbackWindowFrames > 4096)
        result.errors.emplace_back("rollbackWindowFrames must be within [1, 4096]");
    if (settings.inputDelayFrames > settings.rollbackWindowFrames)
        result.errors.emplace_back("inputDelayFrames must not exceed rollbackWindowFrames");
    if (settings.role == ProductionSessionRole::DedicatedServer && settings.maxPlayers < 2)
        result.errors.emplace_back("dedicated server requires at least two player slots");
    if (!settings.encryptedTransport)
        result.warnings.emplace_back("transport encryption is disabled; use only for trusted local sessions");
    if (!settings.enableAntiCheatHooks)
        result.warnings.emplace_back("anti-cheat hooks are disabled; server-side validation is still required");

    result.valid = result.errors.empty();
    return result;
}

bool ProductionOnlineSession::Configure(const ProductionOnlineSessionSettings& settings, std::string* error)
{
    const ProductionOnlineSessionValidation validation = Validate(settings);
    if (!validation.valid)
    {
        SetError(error, validation.errors.front().c_str());
        return false;
    }
    settings_ = settings;
    peers_.clear();
    configured_ = true;
    return true;
}

bool ProductionOnlineSession::AddPeer(const std::string& peerId, bool handshakeAuthenticated)
{
    if (!configured_ || peerId.empty() || peers_.size() >= settings_.maxPlayers)
        return false;
    if (settings_.requireHandshake && !handshakeAuthenticated)
        return false;
    return peers_.insert(peerId).second;
}

bool ProductionOnlineSession::RemovePeer(const std::string& peerId)
{
    return peers_.erase(peerId) != 0;
}

bool ProductionOnlineSession::HasPeer(const std::string& peerId) const
{
    return peers_.find(peerId) != peers_.end();
}

void ProductionOnlineSession::ClearPeers()
{
    peers_.clear();
}

} // namespace Urho3D
