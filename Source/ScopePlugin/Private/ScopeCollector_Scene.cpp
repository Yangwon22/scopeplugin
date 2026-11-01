// ScopeCollector_Scene.cpp
// Collects PostProcessVolume, lighting, level name, and FPS data
// from the editor world using standard UE actor iterators.

#include "ScopeCollector.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "Engine/DirectionalLight.h"
#include "Components/SkyLightComponent.h"
#include "Components/LightComponent.h"
#include "Misc/App.h"

// ---------------------------------------------------------------------------
// CollectScene
// Reads scene-level metadata: level name, fixed FPS, post-process settings,
// sky light intensity, and directional light transform/intensity.
// SequenceName is intentionally left empty — Sequencer API coupling avoided.
// ---------------------------------------------------------------------------
FScopeSceneData FScopeCollector::CollectScene()
{
    FScopeSceneData Data;

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        return Data;
    }

    // ── Level name ───────────────────────────────────────────────────────────
    Data.LevelName = World->GetMapName();
    Data.LevelName.RemoveFromStart(TEXT("UEDPIE_0_"));

    // ── Render FPS from fixed delta time ─────────────────────────────────────
    const float FixedDelta = static_cast<float>(FApp::GetFixedDeltaTime());
    Data.RenderFPS = (FixedDelta > SMALL_NUMBER) ? (1.f / FixedDelta) : 0.f;

    // ── Sequence name: Sequencer API dependency removed ───────────────────────
    // TODO: inject sequence name externally or bind via LevelSequenceActor iterator
    Data.SequenceName = TEXT("");

    // ── Post Process Volume ───────────────────────────────────────────────────
    for (TActorIterator<APostProcessVolume> It(World); It; ++It)
    {
        APostProcessVolume* PPV = *It;
        if (!IsValid(PPV))
        {
            continue;
        }

        // Use only the first enabled, unbound (global) volume
        if (!PPV->bEnabled || !PPV->bUnbound)
        {
            continue;
        }

        const FPostProcessSettings& Settings = PPV->Settings;
        Data.Bloom    = Settings.BloomIntensity;
        Data.Vignette = Settings.VignetteIntensity;
        Data.Exposure = Settings.AutoExposureBias;
        break;
    }

    // ── Sky Light ─────────────────────────────────────────────────────────────
    for (TActorIterator<ASkyLight> It(World); It; ++It)
    {
        ASkyLight* SkyLightActor = *It;
        if (!IsValid(SkyLightActor))
        {
            continue;
        }

        USkyLightComponent* SkyComp = SkyLightActor->GetLightComponent();
        if (SkyComp)
        {
            Data.SkyLightIntensity = SkyComp->Intensity;
        }
        break;
    }

    // ── Directional Light ─────────────────────────────────────────────────────
    for (TActorIterator<ADirectionalLight> It(World); It; ++It)
    {
        ADirectionalLight* DirLightActor = *It;
        if (!IsValid(DirLightActor))
        {
            continue;
        }

        ULightComponent* LightComp = DirLightActor->GetLightComponent();
        if (LightComp)
        {
            Data.DirectionalLightIntensity = LightComp->Intensity;
        }
        Data.DirectionalLightRotation = DirLightActor->GetActorRotation();
        break;
    }

    return Data;
}
