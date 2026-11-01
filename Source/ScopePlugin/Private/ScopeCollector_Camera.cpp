// ScopeCollector_Camera.cpp
// Collects CineCameraActor properties and Live Link tracking data.

#include "ScopeCollector.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "ILiveLinkClient.h"
#include "Features/IModularFeatures.h"
#include "Roles/LiveLinkTransformRole.h"
#include "Roles/LiveLinkTransformTypes.h"

// ---------------------------------------------------------------------------
// CollectCamera
// Finds the first ACineCameraActor in the editor world and reads its
// transform and lens/filmback settings from UCineCameraComponent.
// ---------------------------------------------------------------------------
FScopeCameraData FScopeCollector::CollectCamera()
{
    FScopeCameraData Data;

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        return Data;
    }

    for (TActorIterator<ACineCameraActor> It(World); It; ++It)
    {
        ACineCameraActor* CamActor = *It;
        if (!IsValid(CamActor))
        {
            continue;
        }

        // World transform — use root component to correctly resolve parent attachment
        USceneComponent* Root = CamActor->GetRootComponent();
        Data.Location = Root ? Root->GetComponentLocation() : CamActor->GetActorLocation();
        Data.Rotation = Root ? Root->GetComponentRotation() : CamActor->GetActorRotation();

        // Lens / filmback settings from the camera component
        UCineCameraComponent* CamComp = CamActor->GetCineCameraComponent();
        if (CamComp)
        {
            Data.FOV           = CamComp->FieldOfView;
            Data.Aperture      = CamComp->CurrentAperture;
            Data.FocusDistance = CamComp->CurrentFocusDistance;
            Data.FocalLength   = CamComp->CurrentFocalLength;
            Data.SensorWidth   = CamComp->Filmback.SensorWidth;
            Data.SensorHeight  = CamComp->Filmback.SensorHeight;
        }

        break; // Use only the first camera found
    }

    return Data;
}

// ---------------------------------------------------------------------------
// CollectTracking
// Queries the Live Link modular feature for available subjects.
// Reads the transform frame from the first active subject.
// ---------------------------------------------------------------------------
FScopeTrackingData FScopeCollector::CollectTracking()
{
    FScopeTrackingData Data;

    // Check whether the Live Link client feature is registered
    const FName FeatureName = ILiveLinkClient::ModularFeatureName;
    if (!IModularFeatures::Get().IsModularFeatureAvailable(FeatureName))
    {
        Data.bLiveLinkActive = false;
        return Data;
    }

    ILiveLinkClient& Client =
        IModularFeatures::Get().GetModularFeature<ILiveLinkClient>(FeatureName);

    // Retrieve enabled subjects (bIncludeDisabled=false, bIncludeVirtualSubjects=true)
    TArray<FLiveLinkSubjectKey> Subjects = Client.GetSubjects(false, true);
    if (Subjects.IsEmpty())
    {
        Data.bLiveLinkActive = false;
        return Data;
    }

    Data.bLiveLinkActive = true;
    Data.SubjectName     = Subjects[0].SubjectName.ToString();

    // Evaluate the latest transform frame for the first subject
    FLiveLinkSubjectFrameData FrameData;
    const bool bGotFrame = Client.EvaluateFrame_AnyThread(
        Subjects[0].SubjectName,
        ULiveLinkTransformRole::StaticClass(),
        FrameData);

    if (bGotFrame)
    {
        const FLiveLinkTransformFrameData* TransformFrame =
            FrameData.FrameData.Cast<FLiveLinkTransformFrameData>();

        if (TransformFrame)
        {
            Data.TrackedLocation = TransformFrame->Transform.GetLocation();
            Data.TrackedRotation = TransformFrame->Transform.Rotator();
        }
    }

    return Data;
}
