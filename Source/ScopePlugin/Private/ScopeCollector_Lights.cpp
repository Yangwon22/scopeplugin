// ScopeCollector_Lights.cpp
// Collects all ALight-derived actors (point, spot, rect, directional, sky)
// with photometric properties from the editor world.

#include "ScopeCollector.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Engine/Light.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Engine/RectLight.h"
#include "Engine/DirectionalLight.h"
#include "Components/LightComponent.h"
#include "Components/LocalLightComponent.h"

// ---------------------------------------------------------------------------
// CollectAllLights
// Iterates all ALight instances in the editor world and reads their
// photometric properties (intensity, color, temperature).
// ---------------------------------------------------------------------------
TArray<FScopeLightEntry> FScopeCollector::CollectAllLights()
{
    TArray<FScopeLightEntry> Result;

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        return Result;
    }

    for (TActorIterator<ALight> It(World); It; ++It)
    {
        ALight* LightActor = *It;
        if (!IsValid(LightActor))
        {
            continue;
        }

        ULightComponent* LightComp = LightActor->GetLightComponent();
        if (!LightComp)
        {
            continue;
        }

        FScopeLightEntry Entry;
        Entry.Name      = LightActor->GetActorLabel();
        Entry.Type      = LightActor->GetClass()->GetName();
        // Stable identifiers for Scene Restore matching (label may be non-unique)
        Entry.Guid      = LightActor->GetActorGuid().ToString();
        Entry.Path      = LightActor->GetPathName();
        if (const AActor* Parent = LightActor->GetAttachParentActor())
        {
            Entry.ParentName   = Parent->GetActorLabel();
            Entry.ParentGuid   = Parent->GetActorGuid().ToString();
            Entry.AttachSocket = LightActor->GetAttachParentSocketName().ToString();
        }
        Entry.Location  = LightActor->GetActorLocation();
        Entry.Rotation  = LightActor->GetActorRotation();
        Entry.Intensity = LightComp->Intensity;
        Entry.Color     = FLinearColor(LightComp->LightColor);

        // Temperature is only valid for local lights (point, spot, rect)
        ULocalLightComponent* LocalLight = Cast<ULocalLightComponent>(LightComp);
        if (LocalLight && LocalLight->bUseTemperature)
        {
            Entry.Temperature = LocalLight->Temperature;
        }

        Result.Add(Entry);
    }

    return Result;
}
