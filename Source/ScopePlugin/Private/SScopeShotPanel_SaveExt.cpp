// SScopeShotPanel_SaveExt.cpp
// Responsibilities: AppendExtendedPayload — builds JSON for 6 extended
//                   data filters (AllActors, StaticMeshes, AllLights,
//                   Blueprints, Sequencer, HDRI) and actor_picker.

#include "SScopeShotPanel.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

// ── Helpers ───────────────────────────────────────────────────────────────────

// Converts an FScopeActorEntry into a JSON object.
static TSharedPtr<FJsonObject> ActorEntryToJson(const FScopeActorEntry& E)
{
    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetStringField(TEXT("name"),  E.Name);
    Obj->SetStringField(TEXT("class"), E.Class);
    Obj->SetStringField(TEXT("guid"),  E.Guid);
    Obj->SetStringField(TEXT("path"),  E.Path);
    Obj->SetStringField(TEXT("parent_name"),   E.ParentName);
    Obj->SetStringField(TEXT("parent_guid"),   E.ParentGuid);
    Obj->SetStringField(TEXT("attach_socket"), E.AttachSocket);

    TSharedPtr<FJsonObject> Loc = MakeShared<FJsonObject>();
    Loc->SetNumberField(TEXT("x"), E.Location.X);
    Loc->SetNumberField(TEXT("y"), E.Location.Y);
    Loc->SetNumberField(TEXT("z"), E.Location.Z);
    Obj->SetObjectField(TEXT("location"), Loc);

    TSharedPtr<FJsonObject> Rot = MakeShared<FJsonObject>();
    Rot->SetNumberField(TEXT("pitch"), E.Rotation.Pitch);
    Rot->SetNumberField(TEXT("yaw"),   E.Rotation.Yaw);
    Rot->SetNumberField(TEXT("roll"),  E.Rotation.Roll);
    Obj->SetObjectField(TEXT("rotation"), Rot);

    TSharedPtr<FJsonObject> Scale = MakeShared<FJsonObject>();
    Scale->SetNumberField(TEXT("x"), E.Scale.X);
    Scale->SetNumberField(TEXT("y"), E.Scale.Y);
    Scale->SetNumberField(TEXT("z"), E.Scale.Z);
    Obj->SetObjectField(TEXT("scale"), Scale);

    return Obj;
}

// Builds a JSON array from a TArray of FScopeActorEntry.
static TArray<TSharedPtr<FJsonValue>> ActorArrayToJson(
    const TArray<FScopeActorEntry>& Entries)
{
    TArray<TSharedPtr<FJsonValue>> Out;
    Out.Reserve(Entries.Num());
    for (const FScopeActorEntry& E : Entries)
    {
        Out.Add(MakeShared<FJsonValueObject>(ActorEntryToJson(E)));
    }
    return Out;
}

// ── AppendExtendedPayload ─────────────────────────────────────────────────────

void SScopeShotPanel::AppendExtendedPayload(
    TSharedPtr<FJsonObject>& Root,
    const FScopeShotPayload& Payload)
{
    // ── all_actors ────────────────────────────────────────────────────────────
    if (Payload.Filters.bAllActors && Payload.AllActors.Num() > 0)
    {
        Root->SetArrayField(TEXT("all_actors"),
            ActorArrayToJson(Payload.AllActors));
    }

    // ── static_meshes ─────────────────────────────────────────────────────────
    if (Payload.Filters.bStaticMeshes && Payload.StaticMeshes.Num() > 0)
    {
        Root->SetArrayField(TEXT("static_meshes"),
            ActorArrayToJson(Payload.StaticMeshes));
    }

    // ── all_lights ────────────────────────────────────────────────────────────
    if (Payload.Filters.bAllLights && Payload.AllLights.Num() > 0)
    {
        TArray<TSharedPtr<FJsonValue>> LightArr;
        LightArr.Reserve(Payload.AllLights.Num());

        for (const FScopeLightEntry& L : Payload.AllLights)
        {
            TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
            Obj->SetStringField(TEXT("name"),      L.Name);
            Obj->SetStringField(TEXT("type"),      L.Type);
            Obj->SetStringField(TEXT("guid"),      L.Guid);
            Obj->SetStringField(TEXT("path"),      L.Path);
            Obj->SetStringField(TEXT("parent_name"),   L.ParentName);
            Obj->SetStringField(TEXT("parent_guid"),   L.ParentGuid);
            Obj->SetStringField(TEXT("attach_socket"), L.AttachSocket);
            Obj->SetNumberField(TEXT("intensity"), L.Intensity);

            TSharedPtr<FJsonObject> Loc = MakeShared<FJsonObject>();
            Loc->SetNumberField(TEXT("x"), L.Location.X);
            Loc->SetNumberField(TEXT("y"), L.Location.Y);
            Loc->SetNumberField(TEXT("z"), L.Location.Z);
            Obj->SetObjectField(TEXT("location"), Loc);

            TSharedPtr<FJsonObject> Rot = MakeShared<FJsonObject>();
            Rot->SetNumberField(TEXT("pitch"), L.Rotation.Pitch);
            Rot->SetNumberField(TEXT("yaw"),   L.Rotation.Yaw);
            Rot->SetNumberField(TEXT("roll"),  L.Rotation.Roll);
            Obj->SetObjectField(TEXT("rotation"), Rot);

            TSharedPtr<FJsonObject> Col = MakeShared<FJsonObject>();
            Col->SetNumberField(TEXT("r"), L.Color.R);
            Col->SetNumberField(TEXT("g"), L.Color.G);
            Col->SetNumberField(TEXT("b"), L.Color.B);
            Obj->SetObjectField(TEXT("color"), Col);

            Obj->SetNumberField(TEXT("temperature"), L.Temperature);

            LightArr.Add(MakeShared<FJsonValueObject>(Obj));
        }

        Root->SetArrayField(TEXT("all_lights"), LightArr);
    }

    // ── blueprints ────────────────────────────────────────────────────────────
    if (Payload.Filters.bBlueprints && Payload.Blueprints.Num() > 0)
    {
        Root->SetArrayField(TEXT("blueprints"),
            ActorArrayToJson(Payload.Blueprints));
    }

    // ── sequencer ─────────────────────────────────────────────────────────────
    if (Payload.Filters.bSequencer)
    {
        TSharedPtr<FJsonObject> Seq = MakeShared<FJsonObject>();
        Seq->SetStringField(TEXT("sequence_name"),  Payload.Sequencer.SequenceName);
        Seq->SetNumberField(TEXT("current_frame"),  Payload.Sequencer.CurrentFrame);
        Seq->SetNumberField(TEXT("current_time"),   Payload.Sequencer.CurrentTime);
        Seq->SetBoolField  (TEXT("is_playing"),     Payload.Sequencer.bIsPlaying);
        Seq->SetNumberField(TEXT("playback_rate"),  Payload.Sequencer.PlaybackRate);
        Root->SetObjectField(TEXT("sequencer"), Seq);
    }

    // ── hdri ──────────────────────────────────────────────────────────────────
    if (Payload.Filters.bHDRI)
    {
        TSharedPtr<FJsonObject> H = MakeShared<FJsonObject>();
        H->SetStringField(TEXT("cubemap_name"),  Payload.HDRI.CubemapAssetName);
        H->SetNumberField(TEXT("rotation_yaw"),  Payload.HDRI.RotationYaw);
        H->SetNumberField(TEXT("intensity"),     Payload.HDRI.Intensity);
        H->SetBoolField  (TEXT("use_hdri"),      Payload.HDRI.bUseHDRI);
        Root->SetObjectField(TEXT("hdri"), H);
    }

    // ── actor_picker ──────────────────────────────────────────────────────────
    if (Payload.ActorPicker.Num() > 0)
    {
        Root->SetArrayField(TEXT("actor_picker"),
            ActorArrayToJson(Payload.ActorPicker));
    }
}
