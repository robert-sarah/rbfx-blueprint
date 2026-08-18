// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#include "WorldFabricRealtimeSession.h"

#include <algorithm>

namespace Urho3D
{

WorldFabricRealtimeSession::WorldFabricRealtimeSession(WorldFabricCollaboration* collaboration, const ea::string& sessionId)
    : collaboration_(collaboration)
    , sessionId_(sessionId.empty() ? "WorldFabric.Session" : sessionId)
{
}

void WorldFabricRealtimeSession::SetError(ea::string* error, const ea::string& message)
{
    lastError_ = message;
    if (error)
        *error = message;
}

WorldFabricPresence* WorldFabricRealtimeSession::FindPresence(const ea::string& clientId)
{
    for (WorldFabricPresence& presence : presence_)
    {
        if (presence.clientId == clientId)
            return &presence;
    }
    return nullptr;
}

const WorldFabricPresence* WorldFabricRealtimeSession::FindPresence(const ea::string& clientId) const
{
    for (const WorldFabricPresence& presence : presence_)
    {
        if (presence.clientId == clientId)
            return &presence;
    }
    return nullptr;
}

bool WorldFabricRealtimeSession::Join(const ea::string& clientId, const ea::string& displayName,
    const ea::string& role, ea::string* error)
{
    if (sessionId_.empty() || clientId.empty() || FindPresence(clientId))
    {
        SetError(error, "A realtime session requires a non-empty unique session and client identifier.");
        return false;
    }
    if (collaboration_ && !collaboration_->AddClient(clientId))
    {
        SetError(error, collaboration_->GetLastError());
        return false;
    }
    presence_.push_back({clientId, displayName.empty() ? clientId : displayName, role.empty() ? "editor" : role, 0, true});
    std::sort(presence_.begin(), presence_.end(), [](const auto& lhs, const auto& rhs) { return lhs.clientId < rhs.clientId; });
    return true;
}

bool WorldFabricRealtimeSession::Leave(const ea::string& clientId, ea::string* error)
{
    auto it = std::find_if(presence_.begin(), presence_.end(), [&clientId](const auto& presence)
    {
        return presence.clientId == clientId;
    });
    if (it == presence_.end())
    {
        SetError(error, "Realtime session client is not connected.");
        return false;
    }
    if (collaboration_ && !collaboration_->RemoveClient(clientId))
    {
        SetError(error, collaboration_->GetLastError());
        return false;
    }
    presence_.erase(it);
    return true;
}

bool WorldFabricRealtimeSession::SubmitLocal(WorldFabricOperation operation, ea::string* error)
{
    if (!collaboration_ || operation.clientId.empty() || !FindPresence(operation.clientId))
    {
        SetError(error, "Local realtime operations require a known connected client and collaboration graph.");
        return false;
    }
    if (!collaboration_->Submit(operation, error))
        return false;

    WorldFabricSessionEnvelope envelope;
    envelope.sessionId = sessionId_;
    envelope.senderId = operation.clientId;
    envelope.sequence = nextSequence_++;
    envelope.baseRevision = collaboration_->GetRevision() > 0 ? collaboration_->GetRevision() - 1 : 0;
    envelope.operation = operation;
    ++lamportClock_;
    FindPresence(operation.clientId)->lastSequence = envelope.sequence;
    outgoing_.push_back(envelope);
    history_.push_back(envelope);
    return true;
}

bool WorldFabricRealtimeSession::Receive(const WorldFabricSessionEnvelope& envelope, ea::string* error)
{
    if (envelope.sessionId != sessionId_ || envelope.senderId.empty() || envelope.sequence == 0)
    {
        SetError(error, "Realtime envelope has an invalid session, sender or sequence.");
        return false;
    }
    if (!FindPresence(envelope.senderId))
    {
        SetError(error, Format("Realtime envelope comes from unknown client '{}'.", envelope.senderId));
        return false;
    }
    lamportClock_ = std::max(lamportClock_, envelope.operation.revision) + 1;
    if (envelope.acknowledgement)
    {
        Acknowledge(envelope.senderId, envelope.sequence);
        return true;
    }
    for (const WorldFabricSessionEnvelope& existing : incoming_)
    {
        if (existing.senderId == envelope.senderId && existing.sequence == envelope.sequence)
            return true;
    }
    incoming_.push_back(envelope);
    std::sort(incoming_.begin(), incoming_.end(), [](const auto& lhs, const auto& rhs)
    {
        if (lhs.baseRevision != rhs.baseRevision)
            return lhs.baseRevision < rhs.baseRevision;
        if (lhs.senderId != rhs.senderId)
            return lhs.senderId < rhs.senderId;
        return lhs.sequence < rhs.sequence;
    });
    return true;
}

bool WorldFabricRealtimeSession::ApplyIncoming(ea::string* error)
{
    if (!collaboration_)
    {
        SetError(error, "Applying realtime operations requires a collaboration layer.");
        return false;
    }
    while (!incoming_.empty())
    {
        const WorldFabricSessionEnvelope envelope = incoming_.front();
        incoming_.erase(incoming_.begin());
        if (!collaboration_->Submit(envelope.operation, error))
            return false;
        history_.push_back(envelope);
        Acknowledge(envelope.senderId, envelope.sequence);
    }
    return true;
}

void WorldFabricRealtimeSession::FlushOutgoing()
{
    if (!sendCallback_)
    {
        outgoing_.clear();
        return;
    }
    const ea::vector<WorldFabricSessionEnvelope> pending = outgoing_;
    outgoing_.clear();
    for (const WorldFabricSessionEnvelope& envelope : pending)
        sendCallback_(envelope);
}

bool WorldFabricRealtimeSession::Acknowledge(const ea::string& senderId, unsigned long long sequence)
{
    for (auto& acknowledgement : acknowledgements_)
    {
        if (acknowledgement.first == senderId)
        {
            acknowledgement.second = std::max(acknowledgement.second, sequence);
            return true;
        }
    }
    acknowledgements_.push_back({senderId, sequence});
    return true;
}

bool WorldFabricRealtimeSession::IsAcknowledged(const ea::string& senderId, unsigned long long sequence) const
{
    for (const auto& acknowledgement : acknowledgements_)
    {
        if (acknowledgement.first == senderId)
            return acknowledgement.second >= sequence;
    }
    return false;
}

} // namespace Urho3D
