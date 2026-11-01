#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Views/SListView.h"
#include "ScopeTypes.h"

class SScopeShotPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SScopeShotPanel) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

private:
    // ── Section builders ──────────────────────────────────────────────────────
    // (SScopeShotPanel.cpp)
    TSharedRef<SWidget> BuildShotSection();
    TSharedRef<SWidget> BuildStatusSection();

    // (SScopeShotPanel_Filter.cpp)
    TSharedRef<SWidget> BuildFilterSection();
    void LoadFilterConfig();
    void SaveFilterConfig();

    // (SScopeShotPanel_Session.cpp) — session fetch UI
    TSharedRef<SWidget> BuildSessionSection();
    void FetchSessions();
    static FString GetServerURL(); // reads GConfig INI [ScopePlugin] ServerURL

    // (SScopeShotPanel_Save.cpp)
    FReply OnSaveClicked();
    TSharedPtr<FJsonObject> BuildJsonPayload(const FScopeShotPayload& Payload);

    // (SScopeShotPanel_Settings.cpp)
    TSharedRef<SWidget> BuildSettingsSection();
    void LoadSettingsConfig();
    void SaveSettingsConfig();
    void LoadPairingConfig();
    void SavePairingConfig();
    FReply PairWithVPStage();

    // (SScopeShotPanel_SaveFlow.cpp)
    FReply OnSaveUe5Clicked();

    // (SScopeShotPanel_ActorPicker.cpp)
    TSharedRef<SWidget> BuildActorPickerSection();
    void RefreshActorList();
    void LoadActorPickerConfig();
    void SaveActorPickerConfig();

    // (SScopeShotPanel_SaveExt.cpp)
    void AppendExtendedPayload(TSharedPtr<FJsonObject>& Root, const FScopeShotPayload& Payload);

    // ── Session combo state ───────────────────────────────────────────────────
    TArray<TSharedPtr<FScopeSession>>              Sessions;
    TSharedPtr<FScopeSession>                      SelectedSession;
    TSharedPtr<SComboBox<TSharedPtr<FScopeSession>>> SessionCombo;

    // ── Shot field widgets ────────────────────────────────────────────────────
    TSharedPtr<SSpinBox<int32>>   ShotBox;
    TSharedPtr<SSpinBox<int32>>   SceneBox;
    TSharedPtr<SSpinBox<int32>>   TakeBox;

    // ── Status combo ──────────────────────────────────────────────────────────
    TArray<TSharedPtr<FString>>              StatusOptions;
    TSharedPtr<SComboBox<TSharedPtr<FString>>> StatusCombo;
    FString                                  SelectedStatus = TEXT("OK");

    // ── Filter state ──────────────────────────────────────────────────────────
    FScopeFilterFlags FilterFlags;

    // ── Settings state ────────────────────────────────────────────────────────
    FString ServerURL = TEXT("http://localhost:3000");
    // Kept in Saved/Config, not ProjectConfig: these belong to this Unreal machine.
    FString V2DeviceId;
    FString PairingCredential;
    FString PairingCode;
    FString LastSavedTakeId;
    int32   RestorePort = 30020; // Scene Restore HTTP listener port
    TSharedPtr<SEditableTextBox> ServerURLBox;
    TSharedPtr<SEditableTextBox> PairingCodeBox;
    TSharedPtr<SEditableTextBox> RestorePortBox;

    // ── Actor Picker state ────────────────────────────────────────────────────
    TArray<TSharedPtr<FString>>                        PickableActorNames;
    TSet<FString>                                      SelectedActorNames;
    TSharedPtr<SListView<TSharedPtr<FString>>>          ActorListView;
};
