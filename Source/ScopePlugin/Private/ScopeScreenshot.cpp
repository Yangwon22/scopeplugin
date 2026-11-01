// ScopeScreenshot.cpp
// CaptureInnerFrustum:
//   1. Moves the active perspective viewport to the first ACineCameraActor's view.
//   2. Fires FScreenshotRequest to a temp path (_tmp_ prefix).
//   3. Ticker A (0.1 s): restores the original viewport view.
//   4. Ticker B (polls every 0.2 s): waits for the temp file, centre-crops it to
//      the camera's filmback aspect ratio, saves to the final path, deletes temp.
//   The uploader polls the final path, so it only sees the cropped image.
// CaptureNDC: FScreenshotRequest on the active editor viewport as-is.

#include "ScopeScreenshot.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"

#include "EngineUtils.h"
#include "Engine/World.h"

#include "CineCameraActor.h"
#include "CineCameraComponent.h"

#include "UnrealClient.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "Containers/Ticker.h"

#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"

// ── Internal helpers ──────────────────────────────────────────────────────────

static ACineCameraActor* FindFirstCineCamera(UWorld* World)
{
    if (!World) return nullptr;
    for (TActorIterator<ACineCameraActor> It(World); It; ++It)
        return *It;
    return nullptr;
}

// Centre-crops Pixels (BGRA, SrcW×SrcH) to TargetAspect (W/H).
// Returns the cropped pixel data and updates OutW/OutH.
static TArray<uint8> CropToAspect(const TArray<uint8>& Pixels,
                                   int32 SrcW, int32 SrcH,
                                   float TargetAspect,
                                   int32& OutW, int32& OutH)
{
    // Fit crop rect inside source image at the target aspect
    OutW = SrcW;
    OutH = FMath::RoundToInt(SrcW / TargetAspect);
    if (OutH > SrcH)
    {
        OutH = SrcH;
        OutW = FMath::RoundToInt(SrcH * TargetAspect);
    }

    const int32 OffX = (SrcW - OutW) / 2;
    const int32 OffY = (SrcH - OutH) / 2;

    TArray<uint8> Cropped;
    Cropped.Reserve(OutW * OutH * 4);
    for (int32 Row = 0; Row < OutH; ++Row)
    {
        const uint8* Src = Pixels.GetData() + ((OffY + Row) * SrcW + OffX) * 4;
        Cropped.Append(Src, OutW * 4);
    }
    return Cropped;
}

static bool CaptureViewportWithCameraView(ACineCameraActor* CineActor,
                                          const FString&    FilePath)
{
    UCineCameraComponent* CamComp = CineActor->GetCineCameraComponent();
    if (!CamComp) return false;

    // Filmback aspect (W/H); fall back to 16:9 if data is missing
    const float SensorW      = CamComp->Filmback.SensorWidth;
    const float SensorH      = CamComp->Filmback.SensorHeight;
    const float TargetAspect = (SensorH > 0.f) ? (SensorW / SensorH) : (16.f / 9.f);

    UE_LOG(LogTemp, Log,
        TEXT("[ScopeScreenshot] Filmback %.2f x %.2f mm → aspect %.4f"),
        SensorW, SensorH, TargetAspect);

    // Find first active perspective viewport client
    FEditorViewportClient* VC = nullptr;
    for (FEditorViewportClient* Client : GEditor->GetAllViewportClients())
    {
        if (Client && Client->IsPerspective()) { VC = Client; break; }
    }
    if (!VC)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[ScopeScreenshot] No perspective viewport client found"));
        return false;
    }

    // Save current view state
    const FVector  SavedLoc = VC->GetViewLocation();
    const FRotator SavedRot = VC->GetViewRotation();
    const float    SavedFOV = VC->ViewFOV;

    // Move viewport to camera view
    VC->SetViewLocation(CineActor->GetActorLocation());
    VC->SetViewRotation(CineActor->GetActorRotation());
    VC->ViewFOV = CamComp->FieldOfView;
    VC->Invalidate();

    // Temp path — uploader polls the final FilePath, so it won't race
    const FString TmpPath = FPaths::Combine(
        FPaths::GetPath(FilePath),
        TEXT("_tmp_") + FPaths::GetCleanFilename(FilePath));

    IFileManager::Get().MakeDirectory(*FPaths::GetPath(TmpPath), /*Tree=*/true);
    FScreenshotRequest::RequestScreenshot(TmpPath, /*bInShowUI=*/false,
                                                   /*bAddFilenameSuffix=*/false);

    UE_LOG(LogTemp, Log, TEXT("[ScopeScreenshot] Requested to temp: %s"), *TmpPath);

    // Ticker A — restore viewport view after 0.1 s (safely after screenshot frame)
    FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateLambda(
            [VC, SavedLoc, SavedRot, SavedFOV](float) -> bool
            {
                VC->SetViewLocation(SavedLoc);
                VC->SetViewRotation(SavedRot);
                VC->ViewFOV = SavedFOV;
                VC->Invalidate();
                return false; // one-shot
            }),
        0.1f);

    // Ticker B — poll for temp file, crop, write final, delete temp
    struct FCropState
    {
        FString TmpPath;
        FString FinalPath;
        float   Aspect;
        float   Elapsed = 0.f;
    };
    TSharedPtr<FCropState> CS = MakeShared<FCropState>();
    CS->TmpPath   = TmpPath;
    CS->FinalPath = FilePath;
    CS->Aspect    = TargetAspect;

    FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateLambda(
            [CS](float Dt) mutable -> bool
            {
                CS->Elapsed += Dt;
                if (CS->Elapsed > 8.f)
                {
                    UE_LOG(LogTemp, Warning,
                        TEXT("[ScopeScreenshot] Timeout waiting for temp file: %s"),
                        *CS->TmpPath);
                    return false;
                }

                if (!IFileManager::Get().FileExists(*CS->TmpPath))
                    return true; // keep polling

                // Load PNG
                TArray<uint8> Raw;
                if (!FFileHelper::LoadFileToArray(Raw, *CS->TmpPath))
                {
                    UE_LOG(LogTemp, Warning,
                        TEXT("[ScopeScreenshot] Failed to load temp file"));
                    return false;
                }

                IImageWrapperModule& IWM =
                    FModuleManager::LoadModuleChecked<IImageWrapperModule>(
                        TEXT("ImageWrapper"));
                TSharedPtr<IImageWrapper> In =
                    IWM.CreateImageWrapper(EImageFormat::PNG);

                if (!In->SetCompressed(Raw.GetData(), Raw.Num()))
                {
                    UE_LOG(LogTemp, Warning,
                        TEXT("[ScopeScreenshot] PNG decode failed"));
                    return false;
                }

                TArray<uint8> Pixels;
                if (!In->GetRaw(ERGBFormat::BGRA, 8, Pixels))
                {
                    UE_LOG(LogTemp, Warning,
                        TEXT("[ScopeScreenshot] GetRaw failed"));
                    return false;
                }

                const int32 SrcW = In->GetWidth();
                const int32 SrcH = In->GetHeight();

                int32 CropW, CropH;
                TArray<uint8> Cropped =
                    CropToAspect(Pixels, SrcW, SrcH, CS->Aspect, CropW, CropH);

                // Re-encode and save final
                TSharedPtr<IImageWrapper> Out =
                    IWM.CreateImageWrapper(EImageFormat::PNG);
                Out->SetRaw(Cropped.GetData(), Cropped.Num(),
                            CropW, CropH, ERGBFormat::BGRA, 8);
                const TArray64<uint8>& PNG = Out->GetCompressed(0);
                FFileHelper::SaveArrayToFile(PNG, *CS->FinalPath);

                IFileManager::Get().Delete(*CS->TmpPath);

                UE_LOG(LogTemp, Log,
                    TEXT("[ScopeScreenshot] InnerFrustum cropped %dx%d → %dx%d saved: %s"),
                    SrcW, SrcH, CropW, CropH, *CS->FinalPath);

                return false; // done
            }),
        0.2f);

    return true;
}

// ── FScopeScreenshot ──────────────────────────────────────────────────────────

FString FScopeScreenshot::MakeFilename(const FString& OutputDir, const FString& Tag)
{
    const FDateTime Now = FDateTime::UtcNow();
    const FString Timestamp = FString::Printf(
        TEXT("%04d%02d%02d_%02d%02d%02d"),
        Now.GetYear(), Now.GetMonth(),  Now.GetDay(),
        Now.GetHour(), Now.GetMinute(), Now.GetSecond());

    return FPaths::Combine(OutputDir,
        FString::Printf(TEXT("%s_%s.png"), *Tag, *Timestamp));
}

void FScopeScreenshot::TriggerCapture(const FString& Filepath)
{
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filepath), /*Tree=*/true);
    FScreenshotRequest::RequestScreenshot(Filepath,
        /*bInShowUI=*/false, /*bAddFilenameSuffix=*/false);
}

// ── Public API ────────────────────────────────────────────────────────────────

FString FScopeScreenshot::CaptureInnerFrustum(const FString& OutputDir)
{
    const FString FilePath = MakeFilename(OutputDir, TEXT("InnerFrustum"));

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (World)
    {
        if (ACineCameraActor* CineActor = FindFirstCineCamera(World))
        {
            UE_LOG(LogTemp, Log, TEXT("[ScopeScreenshot] Using CineCameraActor: %s"),
                *CineActor->GetActorLabel());

            if (CaptureViewportWithCameraView(CineActor, FilePath))
                return FilePath;
        }
        else
        {
            UE_LOG(LogTemp, Log,
                TEXT("[ScopeScreenshot] No ACineCameraActor — using viewport fallback"));
        }
    }

    TriggerCapture(FilePath);
    return FilePath;
}

FString FScopeScreenshot::CaptureNDC(const FString& OutputDir)
{
    const FString FilePath = MakeFilename(OutputDir, TEXT("NDC"));
    TriggerCapture(FilePath);
    return FilePath;
}
