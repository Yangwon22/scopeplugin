// SScopeShotPanel_Save.cpp
// Responsibilities: OnSaveClicked (collect → JSON → screenshot → HTTP POST), BuildJsonPayload

#include "SScopeShotPanel.h"
#include "ScopeCollector.h"
#include "ScopeHttpSender.h"
#include "ScopeScreenshot.h"
#include "ScopeScreenshotUploader.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"

// ── OnSaveClicked ─────────────────────────────────────────────────────────────

FReply SScopeShotPanel::OnSaveClicked()
{
    UE_LOG(LogTemp, Log,
        TEXT("[ScopeActorPicker] OnSave: SelectedActorNames.Num() = %d"),
        SelectedActorNames.Num());

    // 1. Build payload from UI widgets
    FScopeShotPayload Payload;

    Payload.SessionId = SelectedSession.IsValid()
        ? SelectedSession->Id
        : TEXT("");

    Payload.Shot  = ShotBox.IsValid()  ? ShotBox->GetValue()  : 1;
    Payload.Scene = SceneBox.IsValid() ? FString::FromInt(SceneBox->GetValue()) : TEXT("1");
    Payload.Take  = TakeBox.IsValid()  ? TakeBox->GetValue()  : 1;

    Payload.Status    = SelectedStatus;
    Payload.Timestamp = FDateTime::UtcNow().ToIso8601();
    Payload.Filters   = FilterFlags;

    // 2. Collect engine data per active filter
    if (FilterFlags.bCamera)
    {
        Payload.Camera = FScopeCollector::CollectCamera();
    }
    if (FilterFlags.bTracking)
    {
        Payload.Tracking = FScopeCollector::CollectTracking();
    }
    if (FilterFlags.bICVFX)
    {
        Payload.ICVFX = FScopeCollector::CollectICVFX();
    }
    if (FilterFlags.bNDisplay)
    {
        Payload.NDisplay = FScopeCollector::CollectNDisplay();
    }
    if (FilterFlags.bScene)
    {
        Payload.SceneInfo = FScopeCollector::CollectScene();
    }

    // Collect extended filter data
    if (FilterFlags.bAllActors)    { Payload.AllActors    = FScopeCollector::CollectAllActors();    }
    if (FilterFlags.bStaticMeshes) { Payload.StaticMeshes = FScopeCollector::CollectStaticMeshes(); }
    if (FilterFlags.bAllLights)    { Payload.AllLights    = FScopeCollector::CollectAllLights();    }
    if (FilterFlags.bBlueprints)   { Payload.Blueprints   = FScopeCollector::CollectBlueprints();   }
    if (FilterFlags.bSequencer)    { Payload.Sequencer    = FScopeCollector::CollectSequencer();    }
    if (FilterFlags.bHDRI)         { Payload.HDRI         = FScopeCollector::CollectHDRI();         }
    // Always collect selected actors if any are picked
    if (SelectedActorNames.Num() > 0)
    {
        Payload.ActorPicker = FScopeCollector::CollectSelectedActors(SelectedActorNames);
    }

    // 3. Capture screenshots (fire-and-forget; file written next frame)
    const FString ScreenshotDir =
        FPaths::ProjectSavedDir() / TEXT("ScopePlugin/Screenshots");
    const FString NdcPath   = FScopeScreenshot::CaptureNDC(ScreenshotDir);
    const FString InnerPath = FScopeScreenshot::CaptureInnerFrustum(ScreenshotDir);

    // 4. Build JSON root now (thumbnail URLs added inside callback after upload)
    TSharedPtr<FJsonObject> Root = BuildJsonPayload(Payload);

    // 5. Poll for screenshot files, upload them, then POST shot-save with thumbnail URLs
    FScopeScreenshotUploader::UploadAfterCapture(
        NdcPath, InnerPath,
        [Root](FString NdcUrl, FString InnerUrl)
        {
            if (!NdcUrl.IsEmpty())   Root->SetStringField(TEXT("thumb_ndc"),   NdcUrl);
            if (!InnerUrl.IsEmpty()) Root->SetStringField(TEXT("thumb_inner"), InnerUrl);

            FString JsonStr;
            TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonStr);
            FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

            // Log metadata only — full payload omitted to avoid leaking scene geometry in logs
            UE_LOG(LogTemp, Log, TEXT("[ScopePlugin] Posting shot data (%d bytes)"), JsonStr.Len());
            FScopeHttpSender::PostShotData(
                JsonStr,
                [](bool bSuccess, FString Msg)
                {
                    UE_LOG(LogTemp, Log,
                        TEXT("[ScopePlugin] Shot POST %s: %s"),
                        bSuccess ? TEXT("OK") : TEXT("FAIL"),
                        *Msg);
                });
        });

    return FReply::Handled();
}

// ── BuildJsonPayload ──────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> SScopeShotPanel::BuildJsonPayload(const FScopeShotPayload& Payload)
{
    TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();

    // Top-level fields
    Root->SetStringField(TEXT("session_id"), Payload.SessionId);
    Root->SetNumberField(TEXT("shot"),       Payload.Shot);
    Root->SetStringField(TEXT("scene"),      Payload.Scene);
    Root->SetNumberField(TEXT("take"),       Payload.Take);
    Root->SetStringField(TEXT("status"),     Payload.Status);
    Root->SetStringField(TEXT("timestamp"),  Payload.Timestamp);
    if (!V2DeviceId.IsEmpty() && !PairingCredential.IsEmpty())
    {
        Root->SetStringField(TEXT("device_id"), V2DeviceId);
        Root->SetStringField(TEXT("credential"), PairingCredential);
    }

    // ── Camera ───────────────────────────────────────────────────────────────
    if (Payload.Filters.bCamera)
    {
        TSharedPtr<FJsonObject> Cam = MakeShared<FJsonObject>();
        Cam->SetNumberField(TEXT("fov"),            Payload.Camera.FOV);
        Cam->SetNumberField(TEXT("aperture"),        Payload.Camera.Aperture);
        Cam->SetNumberField(TEXT("focus_distance"),  Payload.Camera.FocusDistance);
        Cam->SetNumberField(TEXT("focal_length"),    Payload.Camera.FocalLength);
        Cam->SetNumberField(TEXT("sensor_width"),    Payload.Camera.SensorWidth);
        Cam->SetNumberField(TEXT("sensor_height"),   Payload.Camera.SensorHeight);

        TSharedPtr<FJsonObject> Loc = MakeShared<FJsonObject>();
        Loc->SetNumberField(TEXT("x"), Payload.Camera.Location.X);
        Loc->SetNumberField(TEXT("y"), Payload.Camera.Location.Y);
        Loc->SetNumberField(TEXT("z"), Payload.Camera.Location.Z);
        Cam->SetObjectField(TEXT("location"), Loc);

        TSharedPtr<FJsonObject> Rot = MakeShared<FJsonObject>();
        Rot->SetNumberField(TEXT("pitch"), Payload.Camera.Rotation.Pitch);
        Rot->SetNumberField(TEXT("yaw"),   Payload.Camera.Rotation.Yaw);
        Rot->SetNumberField(TEXT("roll"),  Payload.Camera.Rotation.Roll);
        Cam->SetObjectField(TEXT("rotation"), Rot);

        Root->SetObjectField(TEXT("camera"), Cam);
    }

    // ── Tracking ──────────────────────────────────────────────────────────────
    if (Payload.Filters.bTracking)
    {
        TSharedPtr<FJsonObject> Trk = MakeShared<FJsonObject>();
        Trk->SetBoolField  (TEXT("live_link_active"), Payload.Tracking.bLiveLinkActive);
        Trk->SetStringField(TEXT("subject_name"),     Payload.Tracking.SubjectName);

        TSharedPtr<FJsonObject> TLoc = MakeShared<FJsonObject>();
        TLoc->SetNumberField(TEXT("x"), Payload.Tracking.TrackedLocation.X);
        TLoc->SetNumberField(TEXT("y"), Payload.Tracking.TrackedLocation.Y);
        TLoc->SetNumberField(TEXT("z"), Payload.Tracking.TrackedLocation.Z);
        Trk->SetObjectField(TEXT("location"), TLoc);

        TSharedPtr<FJsonObject> TRot = MakeShared<FJsonObject>();
        TRot->SetNumberField(TEXT("pitch"), Payload.Tracking.TrackedRotation.Pitch);
        TRot->SetNumberField(TEXT("yaw"),   Payload.Tracking.TrackedRotation.Yaw);
        TRot->SetNumberField(TEXT("roll"),  Payload.Tracking.TrackedRotation.Roll);
        Trk->SetObjectField(TEXT("rotation"), TRot);

        Root->SetObjectField(TEXT("tracking"), Trk);
    }

    // ── ICVFX ─────────────────────────────────────────────────────────────────
    if (Payload.Filters.bICVFX)
    {
        TSharedPtr<FJsonObject> ICV = MakeShared<FJsonObject>();
        ICV->SetNumberField(TEXT("inner_frustum_ratio"), Payload.ICVFX.InnerFrustumRatio);
        ICV->SetNumberField(TEXT("overscan"),            Payload.ICVFX.Overscan);
        ICV->SetBoolField  (TEXT("chromakey_enabled"),   Payload.ICVFX.bChromakeyEnabled);

        TSharedPtr<FJsonObject> SE = MakeShared<FJsonObject>();
        SE->SetNumberField(TEXT("x"), Payload.ICVFX.SoftEdge.X);
        SE->SetNumberField(TEXT("y"), Payload.ICVFX.SoftEdge.Y);
        ICV->SetObjectField(TEXT("soft_edge"), SE);

        // Light cards array
        TArray<TSharedPtr<FJsonValue>> LightCards;
        const int32 CardCount = Payload.ICVFX.LightCardNames.Num();
        for (int32 i = 0; i < CardCount; ++i)
        {
            TSharedPtr<FJsonObject> Card = MakeShared<FJsonObject>();
            Card->SetStringField(TEXT("name"),
                Payload.ICVFX.LightCardNames.IsValidIndex(i)
                    ? Payload.ICVFX.LightCardNames[i] : TEXT(""));
            Card->SetNumberField(TEXT("brightness"),
                Payload.ICVFX.LightCardBrightness.IsValidIndex(i)
                    ? Payload.ICVFX.LightCardBrightness[i] : 0.f);
            Card->SetNumberField(TEXT("color_temp"),
                Payload.ICVFX.LightCardColorTemp.IsValidIndex(i)
                    ? Payload.ICVFX.LightCardColorTemp[i] : 0.f);
            LightCards.Add(MakeShared<FJsonValueObject>(Card));
        }
        ICV->SetArrayField(TEXT("light_cards"), LightCards);

        Root->SetObjectField(TEXT("icvfx"), ICV);
    }

    // ── nDisplay ──────────────────────────────────────────────────────────────
    if (Payload.Filters.bNDisplay)
    {
        TSharedPtr<FJsonObject> ND = MakeShared<FJsonObject>();
        ND->SetStringField(TEXT("config_name"),   Payload.NDisplay.ConfigName);
        ND->SetNumberField(TEXT("render_width"),  Payload.NDisplay.RenderResolution.X);
        ND->SetNumberField(TEXT("render_height"), Payload.NDisplay.RenderResolution.Y);
        ND->SetBoolField  (TEXT("genlock"),        Payload.NDisplay.bGenlockActive);
        ND->SetNumberField(TEXT("render_fps"),     Payload.NDisplay.RenderFPS);

        TSharedPtr<FJsonObject> NDLoc = MakeShared<FJsonObject>();
        NDLoc->SetNumberField(TEXT("x"), Payload.NDisplay.Location.X);
        NDLoc->SetNumberField(TEXT("y"), Payload.NDisplay.Location.Y);
        NDLoc->SetNumberField(TEXT("z"), Payload.NDisplay.Location.Z);
        ND->SetObjectField(TEXT("location"), NDLoc);

        TSharedPtr<FJsonObject> NDRot = MakeShared<FJsonObject>();
        NDRot->SetNumberField(TEXT("pitch"), Payload.NDisplay.Rotation.Pitch);
        NDRot->SetNumberField(TEXT("yaw"),   Payload.NDisplay.Rotation.Yaw);
        NDRot->SetNumberField(TEXT("roll"),  Payload.NDisplay.Rotation.Roll);
        ND->SetObjectField(TEXT("rotation"), NDRot);

        TSharedPtr<FJsonObject> NDScale = MakeShared<FJsonObject>();
        NDScale->SetNumberField(TEXT("x"), Payload.NDisplay.Scale.X);
        NDScale->SetNumberField(TEXT("y"), Payload.NDisplay.Scale.Y);
        NDScale->SetNumberField(TEXT("z"), Payload.NDisplay.Scale.Z);
        ND->SetObjectField(TEXT("scale"), NDScale);

        Root->SetObjectField(TEXT("ndisplay"), ND);
    }

    // ── Scene ─────────────────────────────────────────────────────────────────
    if (Payload.Filters.bScene)
    {
        TSharedPtr<FJsonObject> Sc = MakeShared<FJsonObject>();
        Sc->SetStringField(TEXT("level_name"),                  Payload.SceneInfo.LevelName);
        Sc->SetStringField(TEXT("sequence_name"),               Payload.SceneInfo.SequenceName);
        Sc->SetNumberField(TEXT("render_fps"),                   Payload.SceneInfo.RenderFPS);
        Sc->SetNumberField(TEXT("bloom"),                        Payload.SceneInfo.Bloom);
        Sc->SetNumberField(TEXT("vignette"),                     Payload.SceneInfo.Vignette);
        Sc->SetNumberField(TEXT("exposure"),                     Payload.SceneInfo.Exposure);
        Sc->SetNumberField(TEXT("sky_light_intensity"),          Payload.SceneInfo.SkyLightIntensity);
        Sc->SetNumberField(TEXT("directional_light_intensity"),  Payload.SceneInfo.DirectionalLightIntensity);

        TSharedPtr<FJsonObject> DLR = MakeShared<FJsonObject>();
        DLR->SetNumberField(TEXT("pitch"), Payload.SceneInfo.DirectionalLightRotation.Pitch);
        DLR->SetNumberField(TEXT("yaw"),   Payload.SceneInfo.DirectionalLightRotation.Yaw);
        DLR->SetNumberField(TEXT("roll"),  Payload.SceneInfo.DirectionalLightRotation.Roll);
        Sc->SetObjectField(TEXT("directional_light_rotation"), DLR);

        Root->SetObjectField(TEXT("scene"), Sc);
    }

    // Append extended filter sections (new data filters + actor picker)
    AppendExtendedPayload(Root, Payload);

    return Root;
}
