// ScopeApplier.h
// Scene Restore: applies a scope.scene-restore.v1 payload back onto the editor
// world. Inverse of FScopeCollector. MUST run on the game thread.

#pragma once
#include "CoreMinimal.h"

class UWorld;
class AActor;
class FJsonObject;
class FJsonValue;

// Per-request state shared between ScopeApplier.cpp (actors) and
// ScopeApplier_Scene.cpp (lights + scene-level singletons).
struct FScopeRestoreContext
{
    UWorld* World = nullptr;

    // Actor lookup tables built once from the editor world.
    TMap<FString, AActor*> ByGuid;   // GetActorGuid().ToString() (valid guids only)
    TMap<FString, AActor*> ByPath;   // GetPathName()
    TMap<FString, AActor*> ByLabel;  // GetActorLabel() — first occurrence wins

    TMap<FString, int32> Applied;    // category name -> applied count
    TArray<FString>      Missing;    // identifiers that could not be matched

    // Resolves an entry to a scene actor by guid -> path -> name(label).
    AActor* FindActor(const TSharedPtr<FJsonObject>& Obj) const;
};

class FScopeApplier
{
public:
    // Parses JsonBody and applies it. Returns the response JSON:
    //   { "ok":bool, "applied":{cat:count,...}, "missing":[...] }
    // Call on the game thread (actor mutation requires it).
    static FString ApplySceneRestore(const FString& JsonBody);
};

// ── Shared helpers (implemented in ScopeApplier.cpp) ──────────────────────────
// Read {"x","y","z"} / {"pitch","yaw","roll"} sub-objects; leave Out unchanged on miss.
bool    ScopeJsonGetVec(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, FVector&  Out);
bool    ScopeJsonGetRot(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, FRotator& Out);
// Best identifier for the missing[] list: name -> guid -> path.
FString ScopeMissingLabel(const TSharedPtr<FJsonObject>& Obj);

// ── Scene-level appliers (implemented in ScopeApplier_Scene.cpp) ──────────────
void ScopeApplyLights(FScopeRestoreContext& Ctx, const TArray<TSharedPtr<FJsonValue>>& Lights);
void ScopeApplySceneLevel(FScopeRestoreContext& Ctx, const TSharedPtr<FJsonObject>& Root);
