#pragma once
#include "CoreMinimal.h"

// Schedules high-res screenshots from editor viewports.
// Returns the intended output filepath; actual write happens next frame.
class FScopeScreenshot
{
public:
    // Capture from the active editor viewport (inner frustum / NDC view).
    static FString CaptureInnerFrustum(const FString& OutputDir);
    static FString CaptureNDC(const FString& OutputDir);

private:
    static FString MakeFilename(const FString& OutputDir, const FString& Tag);
    static void    TriggerCapture(const FString& Filepath);
};
