// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#include <Urho3D/Replica/RollbackManager.h>

#include <catch2/catch_amalgamated.hpp>

#include <map>

using namespace Urho3D;

namespace
{

StringVariantMap CreateDuelState()
{
    return {
        {"p1X", Variant(0)},
        {"p2X", Variant(6)},
        {"p1Hp", Variant(100)},
        {"p2Hp", Variant(100)},
        {"round", Variant(1)}
    };
}

StringVariantMap CreateInput(unsigned frame, bool includeRemoteInput)
{
    const int p1Move = frame % 8 == 0 ? 1 : (frame % 8 == 4 ? -1 : 0);
    const int p2Move = includeRemoteInput ? (frame % 6 == 0 ? -1 : (frame % 6 == 3 ? 1 : 0)) : 0;
    const bool p1Attack = frame % 11 == 0;
    const bool p2Attack = includeRemoteInput && frame % 13 == 0;
    return {
        {"p1Move", Variant(p1Move)},
        {"p2Move", Variant(p2Move)},
        {"p1Attack", Variant(p1Attack)},
        {"p2Attack", Variant(p2Attack)}
    };
}

bool SimulateDuelFrame(const StringVariantMap& input, StringVariantMap& state)
{
    int p1X = state.at("p1X").GetInt() + input.at("p1Move").GetInt();
    int p2X = state.at("p2X").GetInt() + input.at("p2Move").GetInt();
    p1X = Clamp(p1X, -20, 20);
    p2X = Clamp(p2X, -20, 20);
    state["p1X"] = Variant(p1X);
    state["p2X"] = Variant(p2X);

    const bool inRange = Abs(p1X - p2X) <= 1;
    if (inRange && input.at("p1Attack").GetBool())
        state["p2Hp"] = Variant(Max(0, state.at("p2Hp").GetInt() - 8));
    if (inRange && input.at("p2Attack").GetBool())
        state["p1Hp"] = Variant(Max(0, state.at("p1Hp").GetInt() - 7));
    return true;
}

bool SameDuelState(const StringVariantMap& left, const StringVariantMap& right)
{
    return left.at("p1X") == right.at("p1X")
        && left.at("p2X") == right.at("p2X")
        && left.at("p1Hp") == right.at("p1Hp")
        && left.at("p2Hp") == right.at("p2Hp")
        && left.at("round") == right.at("round");
}

} // namespace

TEST_CASE("Competitive 1v1 demo converges after delayed remote input rollback")
{
    constexpr unsigned totalFrames = 90;
    constexpr unsigned authorityFrame = 45;

    RollbackManager clientRollback(128);
    clientRollback.SetPredictionWindow(12);
    StringVariantMap clientState = CreateDuelState();
    StringVariantMap serverState = CreateDuelState();
    std::map<unsigned, StringVariantMap> serverHistory;

    for (unsigned frame = 1; frame <= totalFrames; ++frame)
    {
        const StringVariantMap serverInput = CreateInput(frame, true);
        CHECK(SimulateDuelFrame(serverInput, serverState));
        serverHistory.emplace(frame, serverState);

        // The remote player becomes observable on the client only after the checkpoint frame.
        const StringVariantMap clientInput = CreateInput(frame, frame <= authorityFrame);
        clientRollback.RecordInput({static_cast<NetworkFrame>(frame), clientInput});
        CHECK(SimulateDuelFrame(clientInput, clientState));
        clientRollback.SaveState(static_cast<NetworkFrame>(frame), clientState);
    }

    const unsigned long long serverFinalDigest = RollbackManager::ComputeStateDigest(serverHistory.at(totalFrames));
    const unsigned long long predictedFinalDigest =
        RollbackManager::ComputeStateDigest(*clientRollback.FindState(static_cast<NetworkFrame>(totalFrames)));
    CHECK(predictedFinalDigest != serverFinalDigest);

    CHECK_FALSE(clientRollback.ValidateAuthoritativeDigest(static_cast<NetworkFrame>(totalFrames), serverFinalDigest));
    CHECK(clientRollback.GetLastDiagnostics().desyncDetected);
    CHECK(clientRollback.GetLastDiagnostics().firstDivergentFrame == static_cast<NetworkFrame>(totalFrames));

    for (unsigned frame = authorityFrame + 1; frame <= totalFrames; ++frame)
    {
        clientRollback.RecordInput({static_cast<NetworkFrame>(frame), CreateInput(frame, true)});
    }

    StringVariantMap correctedState;
    const unsigned long long checkpointDigest =
        RollbackManager::ComputeStateDigest(serverHistory.at(authorityFrame));
    CHECK(clientRollback.Reconcile(static_cast<NetworkFrame>(authorityFrame), serverHistory.at(authorityFrame),
        checkpointDigest, SimulateDuelFrame, correctedState));
    CHECK(clientRollback.GetLastDiagnostics().predictedInputs == totalFrames - authorityFrame);
    CHECK(clientRollback.GetLastDiagnostics().replayedInputs == totalFrames - authorityFrame);
    CHECK(SameDuelState(correctedState, serverHistory.at(totalFrames)));
    CHECK(clientRollback.GetLastDiagnostics().correctedDigest == serverFinalDigest);
}
