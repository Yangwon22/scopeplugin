// SScopeShotPanel_SaveFlow.cpp
// Responsibilities: OnSaveUe5Clicked — same flow as OnSaveClicked but posts to the UE5 endpoint

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

FReply SScopeShotPanel::OnSaveUe5Clicked()
{
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

    if (FilterFlags.bCamera)   { Payload.Camera    = FScopeCollector::CollectCamera();   }
    if (FilterFlags.bTracking) { Payload.Tracking  = FScopeCollector::CollectTracking(); }
    if (FilterFlags.bICVFX)    { Payload.ICVFX     = FScopeCollector::CollectICVFX();    }
    if (FilterFlags.bNDisplay) { Payload.NDisplay   = FScopeCollector::CollectNDisplay(); }
    if (FilterFlags.bScene)    { Payload.SceneInfo  = FScopeCollector::CollectScene();    }

    if (FilterFlags.bAllActors)    { Payload.AllActors    = FScopeCollector::CollectAllActors();    }
    if (FilterFlags.bStaticMeshes) { Payload.StaticMeshes = FScopeCollector::CollectStaticMeshes(); }
    if (FilterFlags.bAllLights)    { Payload.AllLights    = FScopeCollector::CollectAllLights();    }
    if (FilterFlags.bBlueprints)   { Payload.Blueprints   = FScopeCollector::CollectBlueprints();   }
    if (FilterFlags.bSequencer)    { Payload.Sequencer    = FScopeCollector::CollectSequencer();    }
    if (FilterFlags.bHDRI)         { Payload.HDRI         = FScopeCollector::CollectHDRI();         }
    if (SelectedActorNames.Num() > 0)
    {
        Payload.ActorPicker = FScopeCollector::CollectSelectedActors(SelectedActorNames);
    }

    const FString ScreenshotDir =
        FPaths::ProjectSavedDir() / TEXT("ScopePlugin/Screenshots");
    const FString NdcPath   = FScopeScreenshot::CaptureNDC(ScreenshotDir);
    const FString InnerPath = FScopeScreenshot::CaptureInnerFrustum(ScreenshotDir);

    TSharedPtr<FJsonObject> Root = BuildJsonPayload(Payload);

    FScopeScreenshotUploader::UploadAfterCapture(
        NdcPath, InnerPath,
        [Root](FString NdcUrl, FString InnerUrl)
        {
            if (!NdcUrl.IsEmpty())   Root->SetStringField(TEXT("thumb_ndc"),   NdcUrl);
            if (!InnerUrl.IsEmpty()) Root->SetStringField(TEXT("thumb_inner"), InnerUrl);

            FString JsonStr;
            TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonStr);
            FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

            UE_LOG(LogTemp, Log, TEXT("[ScopePlugin/UE5] POST payload:\n%s"), *JsonStr);
            FScopeHttpSender::PostUe5(
                JsonStr,
                [](bool bSuccess, FString Msg)
                {
                    UE_LOG(LogTemp, Log,
                        TEXT("[ScopePlugin/UE5] Shot POST %s: %s"),
                        bSuccess ? TEXT("OK") : TEXT("FAIL"),
                        *Msg);
                });
        });

    return FReply::Handled();
}
