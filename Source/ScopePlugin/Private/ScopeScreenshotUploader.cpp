// ScopeScreenshotUploader.cpp
// Polls disk for screenshot files then uploads them to the web API via HTTP POST.

#include "ScopeScreenshotUploader.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Base64.h"
#include "Containers/Ticker.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"

// ---------------------------------------------------------------------------
// UploadAfterCapture
// ---------------------------------------------------------------------------

void FScopeScreenshotUploader::UploadAfterCapture(
    const FString& NdcPath,
    const FString& InnerPath,
    FOnComplete    OnComplete)
{
    // Shared state keeps both URLs and a counter; OnComplete fires when Count reaches 2.
    struct FSharedState
    {
        FString NdcUrl;
        FString InnerUrl;
        int32   Count = 0;
    };
    TSharedPtr<FSharedState> State = MakeShared<FSharedState>();

    PollAndUpload(NdcPath, TEXT("ndc"),
        [State, OnComplete](FString Url)
        {
            State->NdcUrl = Url;
            if (++State->Count == 2)
            {
                OnComplete(State->NdcUrl, State->InnerUrl);
            }
        });

    PollAndUpload(InnerPath, TEXT("inner"),
        [State, OnComplete](FString Url)
        {
            State->InnerUrl = Url;
            if (++State->Count == 2)
            {
                OnComplete(State->NdcUrl, State->InnerUrl);
            }
        });
}

// ---------------------------------------------------------------------------
// PollAndUpload
// ---------------------------------------------------------------------------

void FScopeScreenshotUploader::PollAndUpload(
    const FString&           FilePath,
    const FString&           Type,
    TFunction<void(FString)> OnDone)
{
    constexpr float PollTimeout = 8.f;
    struct FPollState
    {
        float Elapsed = 0.f;
    };
    TSharedPtr<FPollState> PollState = MakeShared<FPollState>();

    FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateLambda(
            [FilePath, Type, OnDone, PollState](float DeltaTime) mutable -> bool
            {
                PollState->Elapsed += DeltaTime;

                // File ready once its size is positive (engine has flushed the write).
                if (IFileManager::Get().FileSize(*FilePath) > 0)
                {
                    // Read file bytes from disk.
                    TArray<uint8> Bytes;
                    if (!FFileHelper::LoadFileToArray(Bytes, *FilePath))
                    {
                        UE_LOG(LogTemp, Warning,
                            TEXT("[ScopeUploader] Failed to read file: %s"), *FilePath);
                        OnDone(TEXT(""));
                        return false;
                    }

                    // Encode bytes to Base64 for JSON transport.
                    FString Encoded = FBase64::Encode(Bytes);

                    // Build JSON body: { filename, data, type }
                    TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
                    Body->SetStringField(TEXT("filename"), FPaths::GetCleanFilename(FilePath));
                    Body->SetStringField(TEXT("data"),     Encoded);
                    Body->SetStringField(TEXT("type"),     Type);

                    FString JsonStr;
                    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonStr);
                    FJsonSerializer::Serialize(Body.ToSharedRef(), Writer);

                    // HTTP POST to the upload endpoint.
                    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request =
                        FHttpModule::Get().CreateRequest();
                    Request->SetURL(FScopeScreenshotUploader::UploadEndpoint);
                    Request->SetVerb(TEXT("POST"));
                    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
                    Request->SetContentAsString(JsonStr);

                    Request->OnProcessRequestComplete().BindLambda(
                        [OnDone](FHttpRequestPtr /*Req*/, FHttpResponsePtr Response, bool bOk)
                        {
                            if (!bOk || !Response.IsValid() ||
                                Response->GetResponseCode() != 200)
                            {
                                UE_LOG(LogTemp, Warning,
                                    TEXT("[ScopeUploader] Upload failed"));
                                OnDone(TEXT(""));
                                return;
                            }

                            // Parse { "url": "..." } from the response body.
                            TSharedPtr<FJsonObject> Resp;
                            TSharedRef<TJsonReader<>> Reader =
                                TJsonReaderFactory<>::Create(Response->GetContentAsString());
                            FString Url;
                            if (FJsonSerializer::Deserialize(Reader, Resp) &&
                                Resp.IsValid() &&
                                Resp->TryGetStringField(TEXT("url"), Url))
                            {
                                OnDone(Url);
                            }
                            else
                            {
                                OnDone(TEXT(""));
                            }
                        });

                    Request->ProcessRequest();
                    return false; // Stop ticking — upload dispatched.
                }

                // Hard timeout guard.
                if (PollState->Elapsed >= PollTimeout)
                {
                    UE_LOG(LogTemp, Warning,
                        TEXT("[ScopeUploader] Timeout waiting for: %s"), *FilePath);
                    OnDone(TEXT(""));
                    return false;
                }

                return true; // Keep ticking.
            }));
}
