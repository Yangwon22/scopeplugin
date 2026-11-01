// SScopeShotPanel_Session.cpp
// Responsibilities: GetServerURL, FetchSessions, BuildSessionSection

#include "SScopeShotPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/ConfigCacheIni.h" // GConfig
#include "Misc/Paths.h"          // FPaths::ProjectConfigDir

// Reads ServerURL from Config/ScopePlugin.ini [ScopePlugin] ServerURL.
// Default: http://localhost:3000
FString SScopeShotPanel::GetServerURL()
{
    FString URL = TEXT("http://localhost:3000");
    const FString IniPath = FPaths::ProjectConfigDir() / TEXT("ScopePlugin.ini");
    GConfig->GetString(TEXT("ScopePlugin"), TEXT("ServerURL"), URL, IniPath);
    return URL;
}

void SScopeShotPanel::FetchSessions()
{
    TWeakPtr<SScopeShotPanel> WeakSelf =
        StaticCastSharedRef<SScopeShotPanel>(AsShared());

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req =
        FHttpModule::Get().CreateRequest();
    Req->SetURL(GetServerURL() + TEXT("/api/sessions"));
    Req->SetVerb(TEXT("GET"));

    Req->OnProcessRequestComplete().BindLambda(
        [WeakSelf](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
        {
            TSharedPtr<SScopeShotPanel> Self = WeakSelf.Pin();
            if (!Self.IsValid()) return;

            if (!bOk || !Resp.IsValid() || Resp->GetResponseCode() != 200)
            {
                UE_LOG(LogTemp, Warning, TEXT("[ScopePlugin] GET /api/sessions failed"));
                return;
            }

            // Parse JSON array of session objects
            TArray<TSharedPtr<FJsonValue>> JsonArr;
            TSharedRef<TJsonReader<>> Reader =
                TJsonReaderFactory<>::Create(Resp->GetContentAsString());

            if (!FJsonSerializer::Deserialize(Reader, JsonArr)) return;

            Self->Sessions.Empty();
            for (const TSharedPtr<FJsonValue>& Val : JsonArr)
            {
                const TSharedPtr<FJsonObject>* ObjPtr;
                if (!Val->TryGetObject(ObjPtr)) continue;

                TSharedPtr<FScopeSession> S = MakeShared<FScopeSession>();
                // TryGetStringField leaves out-param unchanged on miss — safe for partial responses
                (*ObjPtr)->TryGetStringField(TEXT("id"),     S->Id);
                (*ObjPtr)->TryGetStringField(TEXT("date"),   S->Date);
                (*ObjPtr)->TryGetStringField(TEXT("studio"), S->Studio);
                S->DisplayName = FString::Printf(
                    TEXT("%s — %s"), *S->Date, *S->Studio);
                Self->Sessions.Add(S);
            }

            if (Self->SessionCombo.IsValid())
            {
                Self->SessionCombo->RefreshOptions();
                if (Self->Sessions.Num() > 0)
                {
                    Self->SelectedSession = Self->Sessions[0];
                    Self->SessionCombo->SetSelectedItem(Self->SelectedSession);
                }
            }
        });

    Req->ProcessRequest();
}

TSharedRef<SWidget> SScopeShotPanel::BuildSessionSection()
{
    return SNew(SVerticalBox)

        // Section label
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(FMargin(0.f, 0.f, 0.f, 2.f))
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("Session")))
        ]

        // Session combo + Refresh button
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(FMargin(0.f, 0.f, 0.f, 4.f))
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .FillWidth(1.f)
            .Padding(FMargin(0.f, 0.f, 4.f, 0.f))
            [
                SAssignNew(SessionCombo, SComboBox<TSharedPtr<FScopeSession>>)
                .OptionsSource(&Sessions)
                .OnSelectionChanged_Lambda(
                    [this](TSharedPtr<FScopeSession> Item, ESelectInfo::Type)
                    {
                        SelectedSession = Item;
                    })
                .OnGenerateWidget_Lambda(
                    [](TSharedPtr<FScopeSession> Item) -> TSharedRef<SWidget>
                    {
                        return SNew(STextBlock)
                            .Text(FText::FromString(
                                Item.IsValid() ? Item->DisplayName : TEXT("")));
                    })
                [
                    SNew(STextBlock)
                    .Text_Lambda([this]()
                    {
                        return FText::FromString(
                            SelectedSession.IsValid()
                                ? SelectedSession->DisplayName
                                : TEXT("-- Select Session --"));
                    })
                ]
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            [
                SNew(SButton)
                .Text(FText::FromString(TEXT("Refresh")))
                .OnClicked_Lambda([this]() -> FReply
                {
                    FetchSessions();
                    return FReply::Handled();
                })
            ]
        ];
}
