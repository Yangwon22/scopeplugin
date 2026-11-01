// ScopeRestoreServer.cpp

#include "ScopeRestoreServer.h"
#include "ScopeApplier.h"
#include "HttpServerModule.h"
#include "IHttpRouter.h"
#include "HttpServerRequest.h"
#include "HttpServerResponse.h"
#include "HttpServerConstants.h"
#include "HttpRequestHandler.h"
#include "HttpPath.h"

TSharedPtr<IHttpRouter> FScopeRestoreServer::Router;
FHttpRouteHandle        FScopeRestoreServer::RouteHandle;
uint32                  FScopeRestoreServer::BoundPort = 0;
bool                    FScopeRestoreServer::bRunning  = false;

void FScopeRestoreServer::Start(uint32 Port)
{
    if (bRunning) return;

    FHttpServerModule& Module = FHttpServerModule::Get();
    Router = Module.GetHttpRouter(Port);
    if (!Router.IsValid())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[ScopePlugin] Scene Restore: could not get HTTP router on port %u"), Port);
        return;
    }

    RouteHandle = Router->BindRoute(
        FHttpPath(TEXT("/scope/restore")),
        EHttpServerRequestVerbs::VERB_POST,
        FHttpRequestHandler::CreateStatic(&FScopeRestoreServer::HandleRestore));

    if (!RouteHandle.IsValid())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[ScopePlugin] Scene Restore: failed to bind /scope/restore on port %u"), Port);
        Router.Reset();
        return;
    }

    Module.StartAllListeners();
    BoundPort = Port;
    bRunning  = true;
    UE_LOG(LogTemp, Log,
        TEXT("[ScopePlugin] Scene Restore listening on POST :%u/scope/restore"), Port);
}

void FScopeRestoreServer::Stop()
{
    if (Router.IsValid() && RouteHandle.IsValid())
    {
        Router->UnbindRoute(RouteHandle);
    }
    RouteHandle.Reset();
    Router.Reset();
    bRunning = false;
    // Intentionally not calling StopAllListeners() — other systems may share the server.
}

void FScopeRestoreServer::Restart(uint32 Port)
{
    Stop();
    Start(Port);
}

bool FScopeRestoreServer::HandleRestore(
    const FHttpServerRequest& Request,
    const FHttpResultCallback& OnComplete)
{
    // HttpServer dispatches handlers from the core ticker (game thread); actor
    // mutation in FScopeApplier requires it. Fail loud if that ever changes.
    check(IsInGameThread());

    FString Body;
    if (Request.Body.Num() > 0)
    {
        FUTF8ToTCHAR Conv(reinterpret_cast<const ANSICHAR*>(Request.Body.GetData()), Request.Body.Num());
        Body = FString(Conv.Length(), Conv.Get());
    }

    const FString ResponseJson = FScopeApplier::ApplySceneRestore(Body);

    TUniquePtr<FHttpServerResponse> Response =
        FHttpServerResponse::Create(ResponseJson, TEXT("application/json"));
    Response->Code = EHttpServerResponseCodes::Ok;
    OnComplete(MoveTemp(Response));
    return true;
}
