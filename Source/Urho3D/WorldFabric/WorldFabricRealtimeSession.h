// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <Urho3D/WorldFabric/WorldFabricCollaboration.h>

#include <EASTL/functional.h>
#include <EASTL/vector.h>

namespace Urho3D
{

struct URHO3D_API WorldFabricPresence
{
    ea::string clientId;
    ea::string displayName;
    ea::string role;
    unsigned long long lastSequence{};
    bool connected{};
};

struct URHO3D_API WorldFabricSessionEnvelope
{
    ea::string sessionId;
    ea::string senderId;
    unsigned long long sequence{};
    unsigned long long baseRevision{};
    WorldFabricOperation operation;
    bool acknowledgement{};
};

/// Deterministic real-time collaboration session built on the versioned WorldFabric layer.
/// The transport is deliberately injected so TCP, WebSocket, relay and offline queue
/// implementations can share exactly the same merge and acknowledgement semantics.
class URHO3D_API WorldFabricRealtimeSession
{
public:
    using SendCallback = ea::function<void(const WorldFabricSessionEnvelope&)>;

    WorldFabricRealtimeSession(WorldFabricCollaboration* collaboration = nullptr, const ea::string& sessionId = {});

    void SetCollaboration(WorldFabricCollaboration* collaboration) { collaboration_ = collaboration; }
    WorldFabricCollaboration* GetCollaboration() const { return collaboration_; }
    void SetSessionId(const ea::string& sessionId) { sessionId_ = sessionId; }
    const ea::string& GetSessionId() const { return sessionId_; }
    void SetSendCallback(SendCallback callback) { sendCallback_ = ea::move(callback); }

    bool Join(const ea::string& clientId, const ea::string& displayName = {}, const ea::string& role = "editor",
        ea::string* error = nullptr);
    bool Leave(const ea::string& clientId, ea::string* error = nullptr);
    bool SubmitLocal(WorldFabricOperation operation, ea::string* error = nullptr);
    bool Receive(const WorldFabricSessionEnvelope& envelope, ea::string* error = nullptr);
    bool ApplyIncoming(ea::string* error = nullptr);
    void FlushOutgoing();

    bool Acknowledge(const ea::string& senderId, unsigned long long sequence);
    bool IsAcknowledged(const ea::string& senderId, unsigned long long sequence) const;

    const ea::vector<WorldFabricPresence>& GetPresence() const { return presence_; }
    const ea::vector<WorldFabricSessionEnvelope>& GetPendingOutgoing() const { return outgoing_; }
    const ea::vector<WorldFabricSessionEnvelope>& GetPendingIncoming() const { return incoming_; }
    const ea::vector<WorldFabricSessionEnvelope>& GetHistory() const { return history_; }
    unsigned long long GetNextSequence() const { return nextSequence_; }
    unsigned long long GetLamportClock() const { return lamportClock_; }
    const ea::string& GetLastError() const { return lastError_; }

private:
    WorldFabricPresence* FindPresence(const ea::string& clientId);
    const WorldFabricPresence* FindPresence(const ea::string& clientId) const;
    void SetError(ea::string* error, const ea::string& message);

    WorldFabricCollaboration* collaboration_{};
    ea::string sessionId_;
    ea::vector<WorldFabricPresence> presence_;
    ea::vector<WorldFabricSessionEnvelope> outgoing_;
    ea::vector<WorldFabricSessionEnvelope> incoming_;
    ea::vector<WorldFabricSessionEnvelope> history_;
    ea::vector<ea::pair<ea::string, unsigned long long>> acknowledgements_;
    SendCallback sendCallback_;
    unsigned long long nextSequence_{1};
    unsigned long long lamportClock_{};
    ea::string lastError_;
};

} // namespace Urho3D
