// SScopeShotPanel.cpp
// Responsibilities: Construct (full panel assembly), BuildShotSection, BuildStatusSection

#include "SScopeShotPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Input/SButton.h"

void SScopeShotPanel::Construct(const FArguments& InArgs)
{
    // Populate status dropdown options
    StatusOptions.Add(MakeShared<FString>(TEXT("OK")));
    StatusOptions.Add(MakeShared<FString>(TEXT("NG")));
    StatusOptions.Add(MakeShared<FString>(TEXT("CIRCLE")));
    SelectedStatus = TEXT("OK");

    // Load persisted filter flags and settings from INI
    LoadFilterConfig();
    LoadSettingsConfig();
    LoadPairingConfig();

    ChildSlot
    [
        SNew(SScrollBox)
        + SScrollBox::Slot()
        [
            SNew(SBox)
            .Padding(FMargin(8.f))
            [
                SNew(SVerticalBox)

                // 1. Session dropdown + new session creation
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(FMargin(0.f, 0.f, 0.f, 4.f))
                [
                    BuildSessionSection()
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(FMargin(0.f, 4.f))
                [
                    SNew(SSeparator)
                ]

                // 2. Shot / Scene / Take fields
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(FMargin(0.f, 0.f, 0.f, 4.f))
                [
                    BuildShotSection()
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(FMargin(0.f, 4.f))
                [
                    SNew(SSeparator)
                ]

                // Status combo
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(FMargin(0.f, 0.f, 0.f, 4.f))
                [
                    BuildStatusSection()
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(FMargin(0.f, 4.f))
                [
                    SNew(SSeparator)
                ]

                // Data filter checkboxes
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(FMargin(0.f, 0.f, 0.f, 4.f))
                [
                    BuildFilterSection()
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(FMargin(0.f, 4.f))
                [
                    SNew(SSeparator)
                ]

                // Actor Picker
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(FMargin(0.f, 0.f, 0.f, 4.f))
                [
                    BuildActorPickerSection()
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(FMargin(0.f, 4.f))
                [
                    SNew(SSeparator)
                ]

                // Settings (IP addresses)
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(FMargin(0.f, 0.f, 0.f, 4.f))
                [
                    BuildSettingsSection()
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(FMargin(0.f, 4.f))
                [
                    SNew(SSeparator)
                ]

                // Save button
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(FMargin(0.f, 8.f, 0.f, 0.f))
                .HAlign(HAlign_Left)
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("Save UE5")))
                    .OnClicked(this, &SScopeShotPanel::OnSaveUe5Clicked)
                ]
            ]
        ]
    ];

    // Fetch session list after UI is built (SessionCombo is now valid)
    FetchSessions();
    RefreshActorList();
    LoadActorPickerConfig();
}

TSharedRef<SWidget> SScopeShotPanel::BuildShotSection()
{
    return SNew(SVerticalBox)

        // ── Shot / Scene / Take label ────────────────────────────────────────────
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(FMargin(0.f, 0.f, 0.f, 2.f))
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("Shot / Scene / Take")))
        ]

        // Spin boxes in a horizontal row
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SHorizontalBox)

            // Shot
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin(0.f, 0.f, 4.f, 0.f))
            [
                SNew(STextBlock).Text(FText::FromString(TEXT("Shot:")))
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(FMargin(0.f, 0.f, 8.f, 0.f))
            [
                SAssignNew(ShotBox, SSpinBox<int32>)
                .Value(1)
                .MinValue(1)
                .MaxValue(9999)
                .MinDesiredWidth(60.f)
            ]

            // Scene
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin(0.f, 0.f, 4.f, 0.f))
            [
                SNew(STextBlock).Text(FText::FromString(TEXT("Scene:")))
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(FMargin(0.f, 0.f, 8.f, 0.f))
            [
                SAssignNew(SceneBox, SSpinBox<int32>)
                .Value(1)
                .MinValue(1)
                .MaxValue(9999)
                .MinDesiredWidth(60.f)
            ]

            // Take
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin(0.f, 0.f, 4.f, 0.f))
            [
                SNew(STextBlock).Text(FText::FromString(TEXT("Take:")))
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            [
                SAssignNew(TakeBox, SSpinBox<int32>)
                .Value(1)
                .MinValue(1)
                .MaxValue(999)
                .MinDesiredWidth(60.f)
            ]
        ];
}

TSharedRef<SWidget> SScopeShotPanel::BuildStatusSection()
{
    return SNew(SVerticalBox)

        // ── Status label ─────────────────────────────────────────────────────────
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(FMargin(0.f, 0.f, 0.f, 2.f))
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("Status")))
        ]

        // Status combo box
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SAssignNew(StatusCombo, SComboBox<TSharedPtr<FString>>)
            .OptionsSource(&StatusOptions)
            .OnSelectionChanged_Lambda(
                [this](TSharedPtr<FString> Item, ESelectInfo::Type /*SelectInfo*/)
                {
                    if (Item.IsValid())
                    {
                        SelectedStatus = *Item;
                    }
                })
            .OnGenerateWidget_Lambda(
                [](TSharedPtr<FString> Item) -> TSharedRef<SWidget>
                {
                    return SNew(STextBlock)
                        .Text(FText::FromString(Item.IsValid() ? *Item : TEXT("")));
                })
            [
                // Content shown in the collapsed button
                SNew(STextBlock)
                .Text_Lambda([this]()
                {
                    return FText::FromString(SelectedStatus);
                })
            ]
        ];
}
