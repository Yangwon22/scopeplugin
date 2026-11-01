// SScopeShotPanel_ActorPicker.cpp
// Responsibilities: BuildActorPickerSection, RefreshActorList,
//                   LoadActorPickerConfig, SaveActorPickerConfig

#include "SScopeShotPanel.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"

// INI constants — shares the same file as FilterConfig
static const TCHAR* kActorPickerSection = TEXT("ScopePlugin_ActorPicker");
static const TCHAR* kActorPickerKey     = TEXT("SelectedActors");

// Reuse the helper defined in SScopeShotPanel_Filter.cpp
extern FString GetFilterIniPath();

// ── BuildActorPickerSection ───────────────────────────────────────────────────

TSharedRef<SWidget> SScopeShotPanel::BuildActorPickerSection()
{
    return SNew(SBorder)
        .Visibility(EVisibility::Visible)
        .IsEnabled(true)
        [
            SNew(SVerticalBox)

            // ── Header row: label + Refresh button ───────────────────────────
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(FMargin(0.f, 0.f, 0.f, 4.f))
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                .FillWidth(1.f)
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("Actor Picker")))
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(FMargin(0.f, 0.f, 4.f, 0.f))
                [
                    SNew(SButton)
                    .IsEnabled(true)
                    .Text(TAttribute<FText>::CreateLambda([this]() -> FText
                    {
                        const bool bAll = PickableActorNames.Num() > 0 &&
                            SelectedActorNames.Num() >= PickableActorNames.Num();
                        return FText::FromString(bAll ? TEXT("Deselect All") : TEXT("Select All"));
                    }))
                    .OnClicked_Lambda([this]() -> FReply
                    {
                        const bool bAll = PickableActorNames.Num() > 0 &&
                            SelectedActorNames.Num() >= PickableActorNames.Num();
                        if (bAll)
                        {
                            SelectedActorNames.Empty();
                        }
                        else
                        {
                            for (const TSharedPtr<FString>& Name : PickableActorNames)
                            {
                                if (Name.IsValid()) SelectedActorNames.Add(*Name);
                            }
                        }
                        SaveActorPickerConfig();
                        if (ActorListView.IsValid()) ActorListView->RequestListRefresh();
                        return FReply::Handled();
                    })
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SButton)
                    .IsEnabled(true)
                    .Text(FText::FromString(TEXT("Refresh")))
                    .OnClicked_Lambda([this]() -> FReply
                    {
                        RefreshActorList();
                        return FReply::Handled();
                    })
                ]
            ]

            // ── Scrollable actor list ─────────────────────────────────────────
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SBox)
                .HeightOverride(180.f)
                .Visibility(EVisibility::Visible)
                [
                    SAssignNew(ActorListView, SListView<TSharedPtr<FString>>)
                    .ListItemsSource(&PickableActorNames)
                    .SelectionMode(ESelectionMode::Multi)
                    .IsFocusable(true)
                    .OnGenerateRow_Lambda(
                        [this](TSharedPtr<FString> Item,
                               const TSharedRef<STableViewBase>& OwnerTable)
                            -> TSharedRef<ITableRow>
                        {
                            // Capture item by value for lambdas below
                            TSharedPtr<FString> ItemCopy = Item;
                            return SNew(STableRow<TSharedPtr<FString>>, OwnerTable)
                            .Visibility(EVisibility::Visible)
                            [
                                SNew(SCheckBox)
                                .IsFocusable(true)
                                .IsChecked_Lambda([this, ItemCopy]()
                                {
                                    return (ItemCopy.IsValid() &&
                                            SelectedActorNames.Contains(*ItemCopy))
                                        ? ECheckBoxState::Checked
                                        : ECheckBoxState::Unchecked;
                                })
                                .OnCheckStateChanged_Lambda(
                                    [this, ItemCopy](ECheckBoxState NewState)
                                {
                                    if (!ItemCopy.IsValid()) return;
                                    if (NewState == ECheckBoxState::Checked)
                                    {
                                        SelectedActorNames.Add(*ItemCopy);
                                    }
                                    else
                                    {
                                        SelectedActorNames.Remove(*ItemCopy);
                                    }
                                    const FString ActorName = *ItemCopy;
                                    UE_LOG(LogTemp, Log,
                                        TEXT("[ScopeActorPicker] Checked: %s, SelectedActorNames.Num() = %d"),
                                        *ActorName, SelectedActorNames.Num());
                                    SaveActorPickerConfig();
                                })
                                [
                                    SNew(STextBlock)
                                    .Text(FText::FromString(
                                        Item.IsValid() ? *Item : TEXT("")))
                                ]
                            ];
                        })
                ]
            ]
        ];
}

// ── RefreshActorList ──────────────────────────────────────────────────────────

void SScopeShotPanel::RefreshActorList()
{
    PickableActorNames.Reset();

    if (!GEditor) return;

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!IsValid(World)) return;

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (!IsValid(Actor)) continue;

        PickableActorNames.Add(MakeShared<FString>(Actor->GetActorLabel()));
    }

    if (ActorListView.IsValid())
    {
        ActorListView->RequestListRefresh();
    }
}

// ── LoadActorPickerConfig ─────────────────────────────────────────────────────

void SScopeShotPanel::LoadActorPickerConfig()
{
    const FString Ini = GetFilterIniPath();

    FString Raw;
    if (!GConfig->GetString(kActorPickerSection, kActorPickerKey, Raw, Ini))
    {
        return; // Key absent — start with empty selection
    }

    TArray<FString> Parts;
    Raw.ParseIntoArray(Parts, TEXT(";"), /*bCullEmpty=*/true);

    SelectedActorNames.Empty();
    for (const FString& Part : Parts)
    {
        SelectedActorNames.Add(Part);
    }
}

// ── SaveActorPickerConfig ─────────────────────────────────────────────────────

void SScopeShotPanel::SaveActorPickerConfig()
{
    const FString Ini = GetFilterIniPath();

    TArray<FString> Parts = SelectedActorNames.Array();
    const FString   Joined = FString::Join(Parts, TEXT(";"));

    GConfig->SetString(kActorPickerSection, kActorPickerKey, *Joined, Ini);
    GConfig->Flush(false, Ini);
}
