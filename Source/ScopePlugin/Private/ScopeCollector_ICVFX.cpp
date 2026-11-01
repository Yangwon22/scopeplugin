// ScopeCollector_ICVFX.cpp
// Collects nDisplay ICVFX, LightCard, ChromakeyCard, and NDisplay config data
// using UE reflection only — no hard dependency on DisplayCluster plugin.

#include "ScopeCollector.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "UObject/UnrealType.h"
#include "Misc/App.h"

// ---------------------------------------------------------------------------
// CollectICVFX
// Locates DisplayClusterRootActor by class name via reflection, then reads
// ICVFX/InnerFrustum struct properties. LightCards and ChromakeyCards are
// also matched by class name.
// ---------------------------------------------------------------------------
FScopeICVFXData FScopeCollector::CollectICVFX()
{
    FScopeICVFXData Data;

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        return Data;
    }

    // ── DisplayClusterRootActor — read via reflection ─────────────────────────
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (!IsValid(Actor))
        {
            continue;
        }

        if (!Actor->GetClass()->GetName().Contains(TEXT("DisplayClusterRootActor")))
        {
            continue;
        }

        // Walk struct properties on the root actor looking for ICVFX/InnerFrustum data
        for (TFieldIterator<FStructProperty> PropIt(Actor->GetClass()); PropIt; ++PropIt)
        {
            FStructProperty* StructProp = *PropIt;
            const FString PropName = StructProp->GetName();

            if (!PropName.Contains(TEXT("ICVFX")) && !PropName.Contains(TEXT("InnerFrustum")))
            {
                continue;
            }

            if (!StructProp->Struct) { continue; }
            void* StructData = StructProp->ContainerPtrToValuePtr<void>(Actor);

            // Read float sub-properties from the matched struct
            for (TFieldIterator<FFloatProperty> FIt(StructProp->Struct); FIt; ++FIt)
            {
                FFloatProperty* FloatProp = *FIt;
                const FString FieldName   = FloatProp->GetName();
                const float   Value       = FloatProp->GetPropertyValue_InContainer(StructData);

                if (FieldName.Contains(TEXT("Overscan")))
                {
                    Data.Overscan = Value;
                }
                else if (FieldName.Contains(TEXT("Ratio")) || FieldName.Contains(TEXT("Scale")))
                {
                    Data.InnerFrustumRatio = Value;
                }
                else if (FieldName.Contains(TEXT("SoftEdge")))
                {
                    // Map the first occurrence to X, second to Y
                    if (Data.SoftEdge.X == 0.f)
                    {
                        Data.SoftEdge.X = Value;
                    }
                    else
                    {
                        Data.SoftEdge.Y = Value;
                    }
                }
            }
        }

        break; // Process only the first root actor
    }

    // ── LightCards ────────────────────────────────────────────────────────────
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (!IsValid(Actor))
        {
            continue;
        }

        if (!Actor->GetClass()->GetName().Contains(TEXT("LightCard")))
        {
            continue;
        }

        // Record actor label
        Data.LightCardNames.Add(Actor->GetActorLabel());

        // UV position: per-version field varies — default to zero
        Data.LightCardUV.Add(FVector2D::ZeroVector);

        // Read Luminance/Brightness and ColorTemp from float properties
        float FoundBrightness = 0.f;
        float FoundColorTemp  = 0.f;

        for (TFieldIterator<FFloatProperty> FIt(Actor->GetClass()); FIt; ++FIt)
        {
            FFloatProperty* FloatProp = *FIt;
            const FString   FieldName  = FloatProp->GetName();
            const float     Value      = FloatProp->GetPropertyValue_InContainer(Actor);

            if (FieldName.Contains(TEXT("Luminance")) || FieldName.Contains(TEXT("Brightness")))
            {
                FoundBrightness = Value;
            }
            else if (FieldName.Contains(TEXT("ColorTemp")))
            {
                FoundColorTemp = Value;
            }
        }

        Data.LightCardBrightness.Add(FoundBrightness);
        Data.LightCardColorTemp.Add(FoundColorTemp);
    }

    // ── ChromakeyCard — presence check only ───────────────────────────────────
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (!IsValid(Actor))
        {
            continue;
        }

        if (Actor->GetClass()->GetName().Contains(TEXT("ChromakeyCard")))
        {
            Data.bChromakeyEnabled = true;
            break;
        }
    }

    return Data;
}

// ---------------------------------------------------------------------------
// CollectNDisplay
// Reads config name, render resolution, genlock state and FPS from the
// DisplayClusterRootActor via UE property reflection.
// ---------------------------------------------------------------------------
FScopeNDisplayData FScopeCollector::CollectNDisplay()
{
    FScopeNDisplayData Data;

    // Derive FPS from fixed delta time
    Data.RenderFPS = 1.f / FMath::Max(static_cast<float>(FApp::GetFixedDeltaTime()), 0.001f);

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        return Data;
    }

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (!IsValid(Actor))
        {
            continue;
        }

        if (!Actor->GetClass()->GetName().Contains(TEXT("DisplayClusterRootActor")))
        {
            continue;
        }

        UE_LOG(LogTemp, Log, TEXT("[ScopeICVFX] Found nDisplay actor: %s"),
            *Actor->GetActorLabel());

        Data.ConfigName = Actor->GetActorLabel();

        USceneComponent* NDRoot = Actor->GetRootComponent();
        Data.Location = NDRoot ? NDRoot->GetComponentLocation() : Actor->GetActorLocation();
        Data.Rotation = NDRoot ? NDRoot->GetComponentRotation() : Actor->GetActorRotation();
        Data.Scale    = Actor->GetActorScale3D();

        // ── Render resolution: find a struct property containing int X/Y/Width/Height ──
        for (TFieldIterator<FStructProperty> PropIt(Actor->GetClass()); PropIt; ++PropIt)
        {
            FStructProperty* StructProp = *PropIt;
            if (!StructProp->Struct) { continue; }
            void* StructData = StructProp->ContainerPtrToValuePtr<void>(Actor);

            for (TFieldIterator<FIntProperty> IIt(StructProp->Struct); IIt; ++IIt)
            {
                FIntProperty* IntProp  = *IIt;
                const FString FieldName = IntProp->GetName();
                const int32   Value     = IntProp->GetPropertyValue_InContainer(StructData);

                if (FieldName.Contains(TEXT("Width")) || FieldName.Equals(TEXT("X")))
                {
                    Data.RenderResolution.X = Value;
                }
                else if (FieldName.Contains(TEXT("Height")) || FieldName.Equals(TEXT("Y")))
                {
                    Data.RenderResolution.Y = Value;
                }
            }

            // Stop once we have a non-zero resolution
            if (Data.RenderResolution.X > 0 && Data.RenderResolution.Y > 0)
            {
                break;
            }
        }

        // ── Genlock / sync flag ───────────────────────────────────────────────
        for (TFieldIterator<FBoolProperty> BoolIt(Actor->GetClass()); BoolIt; ++BoolIt)
        {
            FBoolProperty* BoolProp  = *BoolIt;
            const FString  FieldName  = BoolProp->GetName();

            if (FieldName.Contains(TEXT("Genlock")) || FieldName.Contains(TEXT("bSync")))
            {
                Data.bGenlockActive =
                    BoolProp->GetPropertyValue_InContainer(Actor);
                break;
            }
        }

        break; // Use only the first root actor
    }

    if (Data.ConfigName.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[ScopeICVFX] nDisplay actor NOT found"));
    }

    return Data;
}
