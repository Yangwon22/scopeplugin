// ScopeHttpSender.cpp

#include "ScopeHttpSender.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

FString FScopeHttpSender::ServerURL = TEXT("http://localhost:3000");

FString FScopeHttpSender::GetServerURL()
{
    FString Base = ServerURL;
    Base.RemoveFromEnd(TEXT("/"));
    return Base;
}

void FScopeHttpSender::PostToEndpoint(
    const FString& Url,
    const FString& JsonPayload,
    TFunction<void(bool, FString)> OnComplete)
{
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request =
        FHttpModule::Get().CreateRequest();

    Request->SetURL(Url);
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetContentAsString(JsonPayload);

    Request->OnProcessRequestComplete().BindLambda(
        [OnComplete](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
        {
            const bool bSuccess =
                bConnected && Response.IsValid() && Response->GetResponseCode() == 200;

            FString Message;
            if (bSuccess)
            {
                Message = Response->GetContentAsString();
            }
            else if (!bConnected)
            {
                Message = TEXT("HTTP connection failed.");
            }
            else if (!Response.IsValid())
            {
                Message = TEXT("Invalid HTTP response.");
            }
            else
            {
                Message = FString::Printf(TEXT("HTTP error %d: %s"),
                    Response->GetResponseCode(), *Response->GetContentAsString());
            }

            if (OnComplete) { OnComplete(bSuccess, Message); }
        });

    Request->ProcessRequest();
}

void FScopeHttpSender::PostShotData(
    const FString& JsonPayload,
    TFunction<void(bool, FString)> OnComplete)
{
    PostToEndpoint(GetServerURL() + TEXT("/api/shot-save"), JsonPayload, MoveTemp(OnComplete));
}

void FScopeHttpSender::PostUe5(
    const FString& JsonPayload,
    TFunction<void(bool, FString)> OnComplete)
{
    PostToEndpoint(GetServerURL() + TEXT("/api/shot-save/ue5"), JsonPayload, MoveTemp(OnComplete));
}
