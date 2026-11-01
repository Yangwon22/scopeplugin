#pragma once
#include "CoreMinimal.h"

// ── Filter flags: which data categories to include in the JSON payload ──────
struct FScopeFilterFlags
{
    bool bCamera      = true;
    bool bTracking    = true;
    bool bICVFX       = true;
    bool bNDisplay    = true;
    bool bScene       = true;
    // Extended data layers
    bool bAllActors    = false;
    bool bStaticMeshes = false;
    bool bAllLights    = false;
    bool bBlueprints   = false;
    bool bSequencer    = false;
    bool bHDRI         = false;
};

// ── Per-category collected data ───────────────────────────────────────────────
struct FScopeCameraData
{
    FVector  Location       = FVector::ZeroVector;
    FRotator Rotation       = FRotator::ZeroRotator;
    float    FOV            = 0.f;
    float    Aperture       = 0.f;
    float    FocusDistance  = 0.f;
    float    FocalLength    = 0.f;
    float    SensorWidth    = 0.f;
    float    SensorHeight   = 0.f;
};

struct FScopeTrackingData
{
    bool     bLiveLinkActive = false;
    FString  SubjectName;
    FVector  TrackedLocation = FVector::ZeroVector;
    FRotator TrackedRotation = FRotator::ZeroRotator;
};

struct FScopeICVFXData
{
    float    InnerFrustumRatio = 1.f;
    float    Overscan          = 0.f;
    FVector2D SoftEdge         = FVector2D::ZeroVector;
    // Light cards
    TArray<FString>   LightCardNames;
    TArray<FVector2D> LightCardUV;
    TArray<float>     LightCardBrightness;
    TArray<float>     LightCardColorTemp;
    // Chromakey
    bool bChromakeyEnabled = false;
};

struct FScopeNDisplayData
{
    FString    ConfigName;
    FIntPoint  RenderResolution = FIntPoint::ZeroValue;
    bool       bGenlockActive   = false;
    float      RenderFPS        = 0.f;
    FVector    Location         = FVector::ZeroVector;
    FRotator   Rotation         = FRotator::ZeroRotator;
    FVector    Scale            = FVector::OneVector;
};

struct FScopeSceneData
{
    FString  LevelName;
    FString  SequenceName;
    float    RenderFPS                  = 0.f;
    // Post Process
    float    Bloom                      = 0.f;
    float    Vignette                   = 0.f;
    float    Exposure                   = 0.f;
    // Lights
    float    SkyLightIntensity          = 0.f;
    float    DirectionalLightIntensity  = 0.f;
    FRotator DirectionalLightRotation   = FRotator::ZeroRotator;
};

// ── Extended data layer structs ───────────────────────────────────────────────

// Generic actor entry: identity, attach parent, and world transform.
// Guid/Path are stable identifiers used by Scene Restore to match actors
// reliably (ActorLabel is not guaranteed unique). Parent fields record the
// attach hierarchy so restore can apply transforms parent-before-child.
struct FScopeActorEntry
{
    FString  Name;          // ActorLabel (Outliner display name)
    FString  Class;
    FString  Guid;          // Actor->GetActorGuid() — stable editor identifier
    FString  Path;          // Actor->GetPathName() — fallback identifier
    FString  ParentName;    // attach parent ActorLabel ("" if unattached)
    FString  ParentGuid;    // attach parent GUID ("" if unattached)
    FString  AttachSocket;  // attach socket name ("" if none)
    FVector  Location = FVector::ZeroVector;
    FRotator Rotation = FRotator::ZeroRotator;
    FVector  Scale    = FVector::OneVector;
};

// Light actor entry with photometric and color properties
struct FScopeLightEntry
{
    FString      Name;
    FString      Type;
    FString      Guid;          // Actor->GetActorGuid() — stable editor identifier
    FString      Path;          // Actor->GetPathName() — fallback identifier
    FString      ParentName;    // attach parent ActorLabel ("" if unattached)
    FString      ParentGuid;    // attach parent GUID ("" if unattached)
    FString      AttachSocket;  // attach socket name ("" if none)
    FVector      Location    = FVector::ZeroVector;
    FRotator     Rotation    = FRotator::ZeroRotator;
    float        Intensity   = 0.f;
    FLinearColor Color       = FLinearColor::White;
    float        Temperature = 6500.f;
};

// Active LevelSequence playback state
struct FScopeSequencerData
{
    FString SequenceName;
    int32   CurrentFrame = 0;
    float   CurrentTime  = 0.f;
    bool    bIsPlaying   = false;
    float   PlaybackRate = 1.f;
};

// SkyLight HDRI cubemap state
struct FScopeHDRIData
{
    FString  CubemapAssetName;
    float    RotationYaw = 0.f;
    float    Intensity   = 0.f;
    bool     bUseHDRI    = false;
};

// ── Session record returned by GET /api/sessions ─────────────────────────────
struct FScopeSession
{
    FString Id;
    FString Date;
    FString Studio;
    FString DisplayName; // e.g. "2025-04-20 — Studio A"
};

// ── Top-level shot payload ───────────────────────────────────────────────────
struct FScopeShotPayload
{
    FString SessionId;
    int32   Shot  = 1;
    FString Scene = TEXT("1");
    int32   Take  = 1;
    FString Status    = TEXT("OK");
    FString Timestamp;

    FScopeFilterFlags  Filters;
    FScopeCameraData   Camera;
    FScopeTrackingData Tracking;
    FScopeICVFXData    ICVFX;
    FScopeNDisplayData NDisplay;
    FScopeSceneData    SceneInfo;

    // Extended data layers
    TArray<FScopeActorEntry>  AllActors;
    TArray<FScopeActorEntry>  StaticMeshes;
    TArray<FScopeLightEntry>  AllLights;
    TArray<FScopeActorEntry>  Blueprints;
    FScopeSequencerData       Sequencer;
    FScopeHDRIData            HDRI;
    TArray<FScopeActorEntry>  ActorPicker;
};
