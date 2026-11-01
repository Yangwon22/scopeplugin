// SScopeShotPanel_Settings.cpp
// Responsibilities: BuildSettingsSection, LoadSettingsConfig, SaveSettingsConfig (GConfig INI)

#include "SScopeShotPanel.h"
#include "ScopeHttpSender.h"
#include "ScopeRestoreServer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

static const TCHAR* kSettingsSection = TEXT("ScopePlugin");
static const TCHAR* kPairingSection = TEXT("ScopePluginPairing");

static FString GetPairingIniPath()
{
    return FPaths::ProjectSavedDir() / TEXT("Config/ScopePluginPairing.ini");
}

// ── LoadSettingsConfig ────────────────────────────────────────────────────────

void SScopeShotPanel::LoadSettingsConfig()
{
    const FString IniPath = FPaths::ProjectConfigDir() / TEXT("ScopePlugin.ini");
    GConfig->GetString(kSettingsSection, TEXT("ServerURL"), ServerURL, IniPath);
    GConfig->GetInt(kSettingsSection, TEXT("RestorePort"), RestorePort, IniPath);
    FScopeHttpSender::ServerURL = ServerURL;
}

// ── SaveSettingsConfig ────────────────────────────────────────────────────────

void SScopeShotPanel::SaveSettingsConfig()
{
    const FString IniPath = FPaths::ProjectConfigDir() / TEXT("ScopePlugin.ini");
    GConfig->SetString(kSettingsSection, TEXT("ServerURL"), *ServerURL, IniPath);
    GConfig->SetInt(kSettingsSection, TEXT("RestorePort"), RestorePort, IniPath);
    GConfig->Flush(false, IniPath);
    FScopeHttpSender::ServerURL = ServerURL;
}

void SScopeShotPanel::LoadPairingConfig()
{
    const FString IniPath = GetPairingIniPath();
    GConfig->GetString(kPairingSection, TEXT("DeviceId"), V2DeviceId, IniPath);
    GConfig->GetString(kPairingSection, TEXT("Credential"), PairingCredential, IniPath);
}

void SScopeShotPanel::SavePairingConfig()
{
    const FString IniPath = GetPairingIniPath();
    GConfig->SetString(kPairingSection, TEXT("DeviceId"), *V2DeviceId, IniPath);
    GConfig->SetString(kPairingSection, TEXT("Credential"), *PairingCredential, IniPath);
    GConfig->Flush(false, IniPath);
}

FReply SScopeShotPanel::PairWithVPStage()
{
    if (PairingCode.TrimStartAndEnd().IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[ScopePlugin] Pairing code is required"));
        return FReply::Handled();
    }

    TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("pairing_code"), PairingCode.TrimStartAndEnd());
    FString Json;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
    FJsonSerializer::Serialize(Body.ToSharedRef(), Writer);

    TWeakPtr<SScopeShotPanel> WeakSelf = StaticCastSharedRef<SScopeShotPanel>(AsShared());
    FScopeHttpSender::PostToEndpoint(
        FScopeHttpSender::GetServerURL() + TEXT("/api/v2/unreal/pair"), Json,
        [WeakSelf](bool bSuccess, FString Response)
        {
            TSharedPtr<SScopeShotPanel> Self = WeakSelf.Pin();
            if (!Self.IsValid()) return;
            if (!bSuccess)
            {
                UE_LOG(LogTemp, Warning, TEXT("[ScopePlugin] Pairing failed: %s"), *Response);
                return;
            }
            TSharedPtr<FJsonObject> Result;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response);
            FString DeviceId, Credential;
            if (!FJsonSerializer::Deserialize(Reader, Result) || !Result.IsValid() ||
                !Result->TryGetStringField(TEXT("device_id"), DeviceId) ||
                !Result->TryGetStringField(TEXT("credential"), Credential))
            {
                UE_LOG(LogTemp, Warning, TEXT("[ScopePlugin] Pairing response was invalid"));
                return;
            }
            Self->V2DeviceId = DeviceId;
            Self->PairingCredential = Credential;
            Self->PairingCode.Empty();
            if (Self->PairingCodeBox.IsValid()) Self->PairingCodeBox->SetText(FText::GetEmpty());
            Self->SavePairingConfig();
            UE_LOG(LogTemp, Log, TEXT("[ScopePlugin] Paired to VP STAGE device %s"), *DeviceId);
        });
    return FReply::Handled();
}

// ── BuildSettingsSection ──────────────────────────────────────────────────────

TSharedRef<SWidget> SScopeShotPanel::BuildSettingsSection()
{
    return SNew(SVerticalBox)

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(FMargin(0.f, 0.f, 0.f, 4.f))
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("Settings")))
        ]

        // One-time pairing code. The issued device identity and credential are
        // stored machine-locally under Saved/Config, never in ProjectConfig.
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(FMargin(0.f, 4.f, 0.f, 0.f))
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 4.f, 0.f))
            [ SNew(STextBlock).Text(FText::FromString(TEXT("Pairing Code:"))) ]
            + SHorizontalBox::Slot().FillWidth(1.f)
            [
                SAssignNew(PairingCodeBox, SEditableTextBox)
                .HintText(FText::FromString(TEXT("Enter VP STAGE pairing code")))
                .OnTextChanged_Lambda([this](const FText& NewText) { PairingCode = NewText.ToString(); })
            ]
            + SHorizontalBox::Slot().AutoWidth().Padding(FMargin(4.f, 0.f, 0.f, 0.f))
            [ SNew(SButton).Text(FText::FromString(TEXT("Pair"))).OnClicked(this, &SScopeShotPanel::PairWithVPStage) ]
        ]

        // Server URL row
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin(0.f, 0.f, 4.f, 0.f))
            [
                SNew(STextBlock).Text(FText::FromString(TEXT("Server URL:")))
            ]

            + SHorizontalBox::Slot()
            .FillWidth(1.f)
            [
                SAssignNew(ServerURLBox, SEditableTextBox)
                .Text(FText::FromString(ServerURL))
                .HintText(FText::FromString(TEXT("http://192.168.1.xxx:3000")))
                .OnTextChanged_Lambda([this](const FText& NewText)
                {
                    ServerURL = NewText.ToString();
                    SaveSettingsConfig();
                })
            ]
        ]

        // Restore Port row (Scene Restore HTTP listener)
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(FMargin(0.f, 2.f, 0.f, 0.f))
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin(0.f, 0.f, 4.f, 0.f))
            [
                SNew(STextBlock).Text(FText::FromString(TEXT("Restore Port:")))
            ]

            + SHorizontalBox::Slot()
            .FillWidth(1.f)
            [
                SAssignNew(RestorePortBox, SEditableTextBox)
                .Text(FText::FromString(FString::FromInt(RestorePort)))
                .HintText(FText::FromString(TEXT("30020")))
                // Rebind the listener only on commit (Enter / focus loss).
                .OnTextCommitted_Lambda([this](const FText& NewText, ETextCommit::Type)
                {
                    const int32 NewPort = FCString::Atoi(*NewText.ToString());
                    if (NewPort <= 0 || NewPort > 65535) return;
                    RestorePort = NewPort;
                    SaveSettingsConfig();
                    FScopeRestoreServer::Restart(static_cast<uint32>(RestorePort));
                })
            ]
        ];
}
