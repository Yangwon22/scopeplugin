// ScopeRestoreServer.h
// Runs an in-editor HTTP listener that receives scope.scene-restore.v1 payloads
// on POST /scope/restore and applies them via FScopeApplier.

#pragma once
#include "CoreMinimal.h"
#include "HttpResultCallback.h"
#include "HttpRouteHandle.h"

class IHttpRouter;
struct FHttpServerRequest;

class FScopeRestoreServer
{
public:
    // Binds POST /scope/restore on Port and starts listening. No-op if running.
    static void Start(uint32 Port);
    // Unbinds the route. Safe to call when not running.
    static void Stop();
    // Stop() then Start(Port) — used when the configured port changes.
    static void Restart(uint32 Port);

    static bool   IsRunning() { return bRunning; }
    static uint32 GetPort()   { return BoundPort; }

private:
    // FHttpRequestHandler callback. Runs on the game thread (HttpServer dispatches
    // via the core ticker), so it may touch GEditor / the world directly.
    static bool HandleRestore(const FHttpServerRequest& Request,
                              const FHttpResultCallback& OnComplete);

    static TSharedPtr<IHttpRouter> Router;
    static FHttpRouteHandle        RouteHandle;
    static uint32                  BoundPort;
    static bool                    bRunning;
};
