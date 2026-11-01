// ScopeApplier.cpp
// Entry point, actor lookup, actor/static_mesh transform application, response.

#include "ScopeApplier.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

// ── Shared JSON helpers ───────────────────────────────────────────────────────

bool ScopeJsonGetVec(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, FVector& Out)
{
    const TSharedPtr<FJsonObject>* V;
    if (!Obj.IsValid() || !Obj->TryGetObjectField(Field, V)) return false;
    (*V)->TryGetNumberField(TEXT("x"), Out.X);
    (*V)->TryGetNumberField(TEXT("y"), Out.Y);
    (*V)->TryGetNumberField(TEXT("z"), Out.Z);
    return true;
}

bool ScopeJsonGetRot(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, FRotator& Out)
{
    const TSharedPtr<FJsonObject>* V;
    if (!Obj.IsValid() || !Obj->TryGetObjectField(Field, V)) return false;
    (*V)->TryGetNumberField(TEXT("pitch"), Out.Pitch);
    (*V)->TryGetNumberField(TEXT("yaw"),   Out.Yaw);
    (*V)->TryGetNumberField(TEXT("roll"),  Out.Roll);
    return true;
}

FString ScopeMissingLabel(const TSharedPtr<FJsonObject>& Obj)
{
    FString S;
    if (Obj->TryGetStringField(TEXT("name"), S) && !S.IsEmpty()) return S;
    if (Obj->TryGetStringField(TEXT("guid"), S) && !S.IsEmpty()) return S;
    if (Obj->TryGetStringField(TEXT("path"), S) && !S.IsEmpty()) return S;
    return TEXT("<unknown>");
}

// ── FindActor: guid -> path -> name ───────────────────────────────────────────

AActor* FScopeRestoreContext::FindActor(const TSharedPtr<FJsonObject>& Obj) const
{
    FString S;
    if (Obj->TryGetStringField(TEXT("guid"), S) && !S.IsEmpty())
        if (AActor* const* A = ByGuid.Find(S)) return *A;
    if (Obj->TryGetStringField(TEXT("path"), S) && !S.IsEmpty())
        if (AActor* const* A = ByPath.Find(S)) return *A;
    if (Obj->TryGetStringField(TEXT("name"), S) && !S.IsEmpty())
        if (AActor* const* A = ByLabel.Find(S)) return *A;
    return nullptr;
}

// ── Local helpers ─────────────────────────────────────────────────────────────

// Number of attach parents up the chain — used to order parents before children.
static int32 AttachDepth(const AActor* A)
{
    int32 Depth = 0;
    for (const AActor* P = A->GetAttachParentActor(); P; P = P->GetAttachParentActor())
    {
        if (++Depth > 64) break; // guard against cycles
    }
    return Depth;
}

// Applies world transforms for an actor array, parents first.
static void ApplyActorTransforms(
    FScopeRestoreContext& Ctx,
    const TSharedPtr<FJsonObject>& Root,
    const TCHAR* Field)
{
    const TArray<TSharedPtr<FJsonValue>>* Arr;
    if (!Root->TryGetArrayField(Field, Arr)) return;

    struct FItem { AActor* Actor; FTransform Xf; };
    TArray<FItem> Items;
    Items.Reserve(Arr->Num());

    for (const TSharedPtr<FJsonValue>& Val : *Arr)
    {
        const TSharedPtr<FJsonObject>& Obj = Val->AsObject();
        if (!Obj.IsValid()) continue;

        AActor* Actor = Ctx.FindActor(Obj);
        if (!Actor)
        {
            Ctx.Missing.Add(ScopeMissingLabel(Obj));
            continue;
        }

        // Seed with current transform so a missing sub-field keeps the live value.
        FVector  Loc   = Actor->GetActorLocation();
        FRotator Rot   = Actor->GetActorRotation();
        FVector  Scale = Actor->GetActorScale3D();
        ScopeJsonGetVec(Obj, TEXT("location"), Loc);
        ScopeJsonGetRot(Obj, TEXT("rotation"), Rot);
        ScopeJsonGetVec(Obj, TEXT("scale"),    Scale);

        Items.Add({ Actor, FTransform(Rot, Loc, Scale) });
    }

    // Parents (shallower attach depth) before children, so setting a parent's
    // world transform never drags an already-placed child off-position.
    Items.Sort([](const FItem& X, const FItem& Y)
    {
        return AttachDepth(X.Actor) < AttachDepth(Y.Actor);
    });

    for (const FItem& It : Items)
    {
        It.Actor->Modify();
        It.Actor->SetActorTransform(It.Xf); // world space
        It.Actor->PostEditMove(true);
        Ctx.Applied.FindOrAdd(Field)++;
    }
}

// Serializes the { ok, applied, missing } response.
static FString BuildResponse(const FScopeRestoreContext& Ctx, bool bOk, const FString& Error)
{
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("ok"), bOk);
    if (!Error.IsEmpty()) Resp->SetStringField(TEXT("error"), Error);

    TSharedPtr<FJsonObject> Applied = MakeShared<FJsonObject>();
    for (const TPair<FString, int32>& Pair : Ctx.Applied)
    {
        Applied->SetNumberField(Pair.Key, Pair.Value);
    }
    Resp->SetObjectField(TEXT("applied"), Applied);

    TArray<TSharedPtr<FJsonValue>> Miss;
    for (const FString& M : Ctx.Missing) Miss.Add(MakeShared<FJsonValueString>(M));
    Resp->SetArrayField(TEXT("missing"), Miss);

    FString Out;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
    FJsonSerializer::Serialize(Resp.ToSharedRef(), Writer);
    return Out;
}

// ── ApplySceneRestore ─────────────────────────────────────────────────────────

FString FScopeApplier::ApplySceneRestore(const FString& JsonBody)
{
    FScopeRestoreContext Ctx;

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonBody);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        return BuildResponse(Ctx, false, TEXT("invalid json"));
    }

    Ctx.World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!Ctx.World)
    {
        return BuildResponse(Ctx, false, TEXT("no editor world"));
    }

    // Build lookup tables once.
    for (TActorIterator<AActor> It(Ctx.World); It; ++It)
    {
        AActor* Actor = *It;
        if (!IsValid(Actor)) continue;

        const FGuid Guid = Actor->GetActorGuid();
        if (Guid.IsValid()) Ctx.ByGuid.Add(Guid.ToString(), Actor);
        Ctx.ByPath.Add(Actor->GetPathName(), Actor);

        const FString Label = Actor->GetActorLabel();
        if (!Ctx.ByLabel.Contains(Label)) Ctx.ByLabel.Add(Label, Actor);
    }

    GEditor->BeginTransaction(NSLOCTEXT("ScopePlugin", "SceneRestore", "Scope Scene Restore"));

    ApplyActorTransforms(Ctx, Root, TEXT("actors"));
    ApplyActorTransforms(Ctx, Root, TEXT("static_meshes"));

    const TArray<TSharedPtr<FJsonValue>>* Lights;
    if (Root->TryGetArrayField(TEXT("lights"), Lights))
    {
        ScopeApplyLights(Ctx, *Lights);
    }

    ScopeApplySceneLevel(Ctx, Root);

    GEditor->EndTransaction();

    int32 Total = 0;
    for (const TPair<FString, int32>& Pair : Ctx.Applied) Total += Pair.Value;
    UE_LOG(LogTemp, Log,
        TEXT("[ScopePlugin] Scene Restore: applied %d object(s), %d missing"),
        Total, Ctx.Missing.Num());

    return BuildResponse(Ctx, true, FString());
}
