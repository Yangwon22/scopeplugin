#pragma once
#include "CoreMinimal.h"

// Waits for screenshot files to be written, then uploads them to the web API.
// Both uploads run concurrently; OnComplete fires when both finish (or timeout).
class FScopeScreenshotUploader
{
public:
    using FOnComplete = TFunction<void(FString /*NdcUrl*/, FString /*InnerUrl*/)>;

    static constexpr const TCHAR* UploadEndpoint =
        TEXT("http://localhost:3000/api/upload-screenshot");

    // Polls disk until NdcPath and InnerPath exist, uploads them, then calls OnComplete.
    static void UploadAfterCapture(
        const FString& NdcPath,
        const FString& InnerPath,
        FOnComplete    OnComplete);

private:
    // Polls until FilePath exists on disk, reads and uploads it, calls OnDone with URL.
    static void PollAndUpload(
        const FString&           FilePath,
        const FString&           Type,
        TFunction<void(FString)> OnDone);
};
