// SPDX-License-Identifier: MIT

#pragma once

#include <Urho3D/Animation/Sequencer.h>
#include <Urho3D/Resource/JSONValue.h>
#include <Urho3D/Resource/Resource.h>

namespace Urho3D
{

/// Loadable and editable production cinematic resource backed by the deterministic Sequencer runtime.
class URHO3D_API SequencerResource : public Resource
{
    URHO3D_OBJECT(SequencerResource, Resource);

public:
    explicit SequencerResource(Context* context);

    /// Return whether the file name uses the sequencer resource extension.
    static bool CheckExtension(const ea::string& fileName);

    /// Return the runtime sequencer represented by this resource.
    Sequencer& GetSequencer() { return sequencer_; }
    const Sequencer& GetSequencer() const { return sequencer_; }

    /// Replace the runtime sequencer represented by this resource.
    void SetSequencer(const Sequencer& sequencer) { sequencer_ = sequencer; }

    /// Convert the resource to/from the stable JSON representation.
    JSONValue ToJSON() const;
    bool FromJSON(const JSONValue& root, ea::string* error = nullptr);

    /// Implement Resource.
    /// @{
    void SerializeInBlock(Archive& archive) override;
    bool BeginLoad(Deserializer& source) override;
    bool Save(Serializer& dest) const override;
    /// @}

private:
    Sequencer sequencer_;
};

} // namespace Urho3D
