// ScopeCollector_HDRI.cpp
// Collects SkyLight HDRI/cubemap state from the first ASkyLight actor
// found in the editor world.

#include "ScopeCollector.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Engine/SkyLight.h"
#include "Engine/TextureCube.h"
#include "Components/SkyLightComponent.h"

// ---------------------------------------------------------------------------
// CollectHDRI
// Reads the cubemap asset name, rotation, intensity, and source-type flag
// from the first valid ASkyLight in the editor world.
// Returns an empty FScopeHDRIData when no SkyLight is present.
// ---------------------------------------------------------------------------
FScopeHDRIData FScopeCollector::CollectHDRI()
{
    FScopeHDRIData Data;

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        return Data;
    }

    // Find the first valid SkyLight actor
    ASkyLight* SkyLightActor = nullptr;
    for (TActorIterator<ASkyLight> It(World); It; ++It)
    {
        if (IsValid(*It))
        {
            SkyLightActor = *It;
            break;
        }
    }

    if (!SkyLightActor)
    {
        return Data;
    }

    USkyLightComponent* Comp = SkyLightActor->GetLightComponent();
    if (!Comp)
    {
        return Data;
    }

    // True only when the source is an explicit cubemap asset
    Data.bUseHDRI = (Comp->SourceType == ESkyLightSourceType::SLS_SpecifiedCubemap);

    // Cubemap asset name is only meaningful when bUseHDRI is true
    if (Data.bUseHDRI && Comp->Cubemap)
    {
        Data.CubemapAssetName = Comp->Cubemap->GetName();
    }

    Data.RotationYaw = Comp->SourceCubemapAngle;
    Data.Intensity   = Comp->Intensity;

    return Data;
}
