// ScopeApplier_Scene.cpp
// Applies lights and scene-level singletons (sky light, directional light,
// HDRI, post process, camera). Inverse of the matching collectors.

#include "ScopeApplier.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/Light.h"
#include "Engine/SkyLight.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PostProcessVolume.h"
#include "Components/LightComponent.h"
#include "Components/SkyLightComponent.h"
#include "CineCameraActor.h"
#include "CineCameraComponent.h"

// Returns the first valid actor of class T in the world, or nullptr.
template <typename T>
static T* FirstActorOfClass(UWorld* World)
{
    for (TActorIterator<T> It(World); It; ++It)
    {
        if (IsValid(*It)) return *It;
    }
    return nullptr;
}

// ── Lights ────────────────────────────────────────────────────────────────────

void ScopeApplyLights(FScopeRestoreContext& Ctx, const TArray<TSharedPtr<FJsonValue>>& Lights)
{
    for (const TSharedPtr<FJsonValue>& Val : Lights)
    {
        const TSharedPtr<FJsonObject>& Obj = Val->AsObject();
        if (!Obj.IsValid()) continue;

        ALight* Light = Cast<ALight>(Ctx.FindActor(Obj));
        if (!Light)
        {
            Ctx.Missing.Add(ScopeMissingLabel(Obj));
            continue;
        }

        Light->Modify();
        FVector  Loc = Light->GetActorLocation();
        FRotator Rot = Light->GetActorRotation();
        ScopeJsonGetVec(Obj, TEXT("location"), Loc);
        ScopeJsonGetRot(Obj, TEXT("rotation"), Rot);
        Light->SetActorLocationAndRotation(Loc, Rot);

        if (ULightComponent* LC = Light->GetLightComponent())
        {
            LC->Modify();
            double V = 0.0;
            if (Obj->TryGetNumberField(TEXT("intensity"), V)) LC->SetIntensity(V);

            const TSharedPtr<FJsonObject>* Col;
            if (Obj->TryGetObjectField(TEXT("color"), Col))
            {
                double R = 0, G = 0, B = 0;
                (*Col)->TryGetNumberField(TEXT("r"), R);
                (*Col)->TryGetNumberField(TEXT("g"), G);
                (*Col)->TryGetNumberField(TEXT("b"), B);
                LC->SetLightColor(FLinearColor(R, G, B)); // bSRGB=true round-trips FColor
            }
            if (Obj->TryGetNumberField(TEXT("temperature"), V) && V > 0.0)
            {
                LC->SetUseTemperature(true);
                LC->SetTemperature(V);
            }
        }
        Light->PostEditMove(true);
        Ctx.Applied.FindOrAdd(TEXT("lights"))++;
    }
}

// ── Scene-level singletons ────────────────────────────────────────────────────

void ScopeApplySceneLevel(FScopeRestoreContext& Ctx, const TSharedPtr<FJsonObject>& Root)
{
    UWorld* World = Ctx.World;
    double V = 0.0;

    // sky_light: intensity
    const TSharedPtr<FJsonObject>* Sky;
    if (Root->TryGetObjectField(TEXT("sky_light"), Sky))
    {
        if (ASkyLight* SkyActor = FirstActorOfClass<ASkyLight>(World))
        {
            if (USkyLightComponent* SC = SkyActor->GetLightComponent())
            {
                SC->Modify();
                if ((*Sky)->TryGetNumberField(TEXT("intensity"), V)) SC->SetIntensity(V);
                SC->SetCaptureIsDirty();
                Ctx.Applied.FindOrAdd(TEXT("sky_light"))++;
            }
        }
        else { Ctx.Missing.Add(TEXT("SkyLight")); }
    }

    // directional_light: rotation + intensity
    const TSharedPtr<FJsonObject>* Dir;
    if (Root->TryGetObjectField(TEXT("directional_light"), Dir))
    {
        if (ADirectionalLight* DL = FirstActorOfClass<ADirectionalLight>(World))
        {
            DL->Modify();
            FRotator Rot = DL->GetActorRotation();
            if (ScopeJsonGetRot(*Dir, TEXT("rotation"), Rot)) DL->SetActorRotation(Rot);
            if (ULightComponent* LC = DL->GetLightComponent())
            {
                LC->Modify();
                if ((*Dir)->TryGetNumberField(TEXT("intensity"), V)) LC->SetIntensity(V);
            }
            DL->PostEditMove(true);
            Ctx.Applied.FindOrAdd(TEXT("directional_light"))++;
        }
        else { Ctx.Missing.Add(TEXT("DirectionalLight")); }
    }

    // hdri: best-effort on the SkyLight (cubemap rotation + intensity).
    // Cubemap asset swap by name is intentionally not performed.
    const TSharedPtr<FJsonObject>* Hdri;
    if (Root->TryGetObjectField(TEXT("hdri"), Hdri))
    {
        if (ASkyLight* SkyActor = FirstActorOfClass<ASkyLight>(World))
        {
            if (USkyLightComponent* SC = SkyActor->GetLightComponent())
            {
                SC->Modify();
                if ((*Hdri)->TryGetNumberField(TEXT("rotation_yaw"), V)) SC->SetSourceCubemapAngle(V);
                if ((*Hdri)->TryGetNumberField(TEXT("intensity"), V))    SC->SetIntensity(V);
                SC->SetCaptureIsDirty();
                Ctx.Applied.FindOrAdd(TEXT("hdri"))++;
            }
        }
    }

    // post_process: bloom / vignette / exposure on the first volume.
    const TSharedPtr<FJsonObject>* PP;
    if (Root->TryGetObjectField(TEXT("post_process"), PP))
    {
        if (APostProcessVolume* Vol = FirstActorOfClass<APostProcessVolume>(World))
        {
            Vol->Modify();
            if ((*PP)->TryGetNumberField(TEXT("bloom"), V))
            {
                Vol->Settings.bOverride_BloomIntensity = true;
                Vol->Settings.BloomIntensity = V;
            }
            if ((*PP)->TryGetNumberField(TEXT("vignette"), V))
            {
                Vol->Settings.bOverride_VignetteIntensity = true;
                Vol->Settings.VignetteIntensity = V;
            }
            if ((*PP)->TryGetNumberField(TEXT("exposure"), V))
            {
                Vol->Settings.bOverride_AutoExposureBias = true;
                Vol->Settings.AutoExposureBias = V;
            }
            Vol->MarkPackageDirty();
            Ctx.Applied.FindOrAdd(TEXT("post_process"))++;
        }
        else { Ctx.Missing.Add(TEXT("PostProcessVolume")); }
    }

    // camera: transform + lens on a CineCameraActor (matched, else first found).
    const TSharedPtr<FJsonObject>* Cam;
    if (Root->TryGetObjectField(TEXT("camera"), Cam))
    {
        ACineCameraActor* CC = Cast<ACineCameraActor>(Ctx.FindActor(*Cam));
        if (!CC) CC = FirstActorOfClass<ACineCameraActor>(World);
        if (CC)
        {
            CC->Modify();
            FVector  Loc = CC->GetActorLocation();
            FRotator Rot = CC->GetActorRotation();
            ScopeJsonGetVec(*Cam, TEXT("location"), Loc);
            ScopeJsonGetRot(*Cam, TEXT("rotation"), Rot);
            CC->SetActorLocationAndRotation(Loc, Rot);

            if (UCineCameraComponent* Comp = CC->GetCineCameraComponent())
            {
                Comp->Modify();
                if ((*Cam)->TryGetNumberField(TEXT("focal_length"), V)) Comp->SetCurrentFocalLength(V);
                if ((*Cam)->TryGetNumberField(TEXT("aperture"), V))     Comp->SetCurrentAperture(V);
                if ((*Cam)->TryGetNumberField(TEXT("focus_distance"), V))
                {
                    FCameraFocusSettings FS = Comp->FocusSettings;
                    FS.ManualFocusDistance = V;
                    Comp->SetFocusSettings(FS);
                }
                double SW = 0, SH = 0;
                const bool bHasW = (*Cam)->TryGetNumberField(TEXT("sensor_width"),  SW);
                const bool bHasH = (*Cam)->TryGetNumberField(TEXT("sensor_height"), SH);
                if (bHasW || bHasH)
                {
                    FCameraFilmbackSettings FB = Comp->Filmback;
                    if (bHasW) FB.SensorWidth  = SW;
                    if (bHasH) FB.SensorHeight = SH;
                    Comp->SetFilmback(FB);
                }
            }
            CC->PostEditMove(true);
            Ctx.Applied.FindOrAdd(TEXT("camera"))++;
        }
        else { Ctx.Missing.Add(TEXT("CineCamera")); }
    }
}
