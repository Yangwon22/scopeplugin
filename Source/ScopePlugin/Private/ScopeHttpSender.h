#pragma once
#include "CoreMinimal.h"

class FScopeHttpSender
{
public:
    static void PostToEndpoint(const FString& Url,
                               const FString& JsonPayload,
                               TFunction<void(bool, FString)> OnComplete);

    static void PostShotData(const FString& JsonPayload,
                             TFunction<void(bool, FString)> OnComplete);

    static void PostUe5(const FString& JsonPayload,
                        TFunction<void(bool, FString)> OnComplete);

    static FString GetServerURL();

    static FString ServerURL;
};
