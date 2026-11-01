// ScopeCollector_Sequencer.cpp
// Collects LevelSequence playback state (frame, time, play rate, is-playing)
// from the first ALevelSequenceActor found in the editor world.

#include "ScopeCollector.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"

// ---------------------------------------------------------------------------
// CollectSequencer
// Finds the first ALevelSequenceActor in the editor world and reads its
// playback state via ULevelSequencePlayer.
// Returns an empty FScopeSequencerData when no sequence actor is present.
// ---------------------------------------------------------------------------
FScopeSequencerData FScopeCollector::CollectSequencer()
{
    FScopeSequencerData Data;

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        return Data;
    }

    // Find the first valid LevelSequenceActor in the world
    ALevelSequenceActor* SeqActor = nullptr;
    for (TActorIterator<ALevelSequenceActor> It(World); It; ++It)
    {
        if (IsValid(*It))
        {
            SeqActor = *It;
            break;
        }
    }

    if (!SeqActor)
    {
        return Data;
    }

    // Populate sequence asset name
    ULevelSequence* Sequence = Cast<ULevelSequence>(SeqActor->GetSequence());
    Data.SequenceName = Sequence ? Sequence->GetName() : TEXT("");

    // Access the runtime player; may be null in editor if never played
    ULevelSequencePlayer* Player = SeqActor->GetSequencePlayer();
    if (!Player)
    {
        return Data;
    }

    Data.bIsPlaying   = Player->IsPlaying();
    Data.PlaybackRate = Player->GetPlayRate();

    // Resolve current position to both frame number and wall-clock seconds
    const FQualifiedFrameTime QFT = Player->GetCurrentTime();
    Data.CurrentFrame = QFT.Time.GetFrame().Value;
    Data.CurrentTime  = static_cast<float>(QFT.AsSeconds());

    return Data;
}
