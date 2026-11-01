// SScopeShotPanel_Filter.cpp
// Responsibilities: BuildFilterSection (11 checkboxes in 2 rows),
//                   LoadFilterConfig, SaveFilterConfig (GConfig INI)

#include "SScopeShotPanel.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"

// INI constants
static const TCHAR* kFilterSection = TEXT("ScopePlugin_Filters");

FString GetFilterIniPath()
{
    return FPaths::ProjectSavedDir() / TEXT("ScopePlugin/FilterConfig.ini");
}

// ── BuildFilterSection ────────────────────────────────────────────────────────

TSharedRef<SWidget> SScopeShotPanel::BuildFilterSection()
{
    // Builds a labelled checkbox bound to a member flag.
    // bFlag is captured by pointer; both pointer and 'this' remain valid for
    // the widget lifetime because flags are members of this panel.
    auto MakeCB = [this](bool& bFlag, const FString& Label, float RightPad = 12.f)
        -> TSharedRef<SWidget>
    {
        bool* FlagPtr = &bFlag;
        return SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(FMargin(0.f, 0.f, RightPad, 0.f))
            [
                SNew(SCheckBox)
                .IsChecked_Lambda([FlagPtr]()
                {
                    return *FlagPtr
                        ? ECheckBoxState::Checked
                        : ECheckBoxState::Unchecked;
                })
                .OnCheckStateChanged_Lambda([this, FlagPtr](ECheckBoxState S)
                {
                    *FlagPtr = (S == ECheckBoxState::Checked);
                    SaveFilterConfig();
                })
                [
                    SNew(STextBlock).Text(FText::FromString(Label))
                ]
            ];
    };

    return SNew(SVerticalBox)

        // ── Section label ─────────────────────────────────────────────────────
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(FMargin(0.f, 0.f, 0.f, 4.f))
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("Data Filters")))
        ]

        // ── Row 1: core filters ───────────────────────────────────────────────
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth() [ MakeCB(FilterFlags.bCamera,   TEXT("Camera"))   ]
            + SHorizontalBox::Slot().AutoWidth() [ MakeCB(FilterFlags.bTracking, TEXT("Tracking")) ]
            + SHorizontalBox::Slot().AutoWidth() [ MakeCB(FilterFlags.bICVFX,    TEXT("ICVFX"))    ]
            + SHorizontalBox::Slot().AutoWidth() [ MakeCB(FilterFlags.bNDisplay, TEXT("nDisplay")) ]
            + SHorizontalBox::Slot().AutoWidth() [ MakeCB(FilterFlags.bScene,    TEXT("Scene"), 0.f) ]
        ]

        // ── Row 2: extended filters ───────────────────────────────────────────
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(FMargin(0.f, 4.f, 0.f, 0.f))
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth() [ MakeCB(FilterFlags.bAllActors,    TEXT("All Actors"))    ]
            + SHorizontalBox::Slot().AutoWidth() [ MakeCB(FilterFlags.bStaticMeshes, TEXT("Static Meshes")) ]
            + SHorizontalBox::Slot().AutoWidth() [ MakeCB(FilterFlags.bAllLights,    TEXT("All Lights"))    ]
            + SHorizontalBox::Slot().AutoWidth() [ MakeCB(FilterFlags.bBlueprints,   TEXT("Blueprints"))    ]
            + SHorizontalBox::Slot().AutoWidth() [ MakeCB(FilterFlags.bSequencer,    TEXT("Sequencer"))     ]
            + SHorizontalBox::Slot().AutoWidth() [ MakeCB(FilterFlags.bHDRI,         TEXT("HDRI"), 0.f)     ]
        ];
}

// ── SaveFilterConfig ──────────────────────────────────────────────────────────

void SScopeShotPanel::SaveFilterConfig()
{
    const FString Ini = GetFilterIniPath();

    GConfig->SetBool(kFilterSection, TEXT("bCamera"),       FilterFlags.bCamera,       Ini);
    GConfig->SetBool(kFilterSection, TEXT("bTracking"),     FilterFlags.bTracking,     Ini);
    GConfig->SetBool(kFilterSection, TEXT("bICVFX"),        FilterFlags.bICVFX,        Ini);
    GConfig->SetBool(kFilterSection, TEXT("bNDisplay"),     FilterFlags.bNDisplay,     Ini);
    GConfig->SetBool(kFilterSection, TEXT("bScene"),        FilterFlags.bScene,        Ini);
    GConfig->SetBool(kFilterSection, TEXT("bAllActors"),    FilterFlags.bAllActors,    Ini);
    GConfig->SetBool(kFilterSection, TEXT("bStaticMeshes"), FilterFlags.bStaticMeshes, Ini);
    GConfig->SetBool(kFilterSection, TEXT("bAllLights"),    FilterFlags.bAllLights,    Ini);
    GConfig->SetBool(kFilterSection, TEXT("bBlueprints"),   FilterFlags.bBlueprints,   Ini);
    GConfig->SetBool(kFilterSection, TEXT("bSequencer"),    FilterFlags.bSequencer,    Ini);
    GConfig->SetBool(kFilterSection, TEXT("bHDRI"),         FilterFlags.bHDRI,         Ini);

    // Flush to disk immediately so values survive editor restarts
    GConfig->Flush(false, Ini);
}

// ── LoadFilterConfig ──────────────────────────────────────────────────────────

void SScopeShotPanel::LoadFilterConfig()
{
    const FString Ini = GetFilterIniPath();

    // GetBool leaves the out-param unchanged when the key is absent,
    // so FScopeFilterFlags defaults are preserved on first run.
    GConfig->GetBool(kFilterSection, TEXT("bCamera"),       FilterFlags.bCamera,       Ini);
    GConfig->GetBool(kFilterSection, TEXT("bTracking"),     FilterFlags.bTracking,     Ini);
    GConfig->GetBool(kFilterSection, TEXT("bICVFX"),        FilterFlags.bICVFX,        Ini);
    GConfig->GetBool(kFilterSection, TEXT("bNDisplay"),     FilterFlags.bNDisplay,     Ini);
    GConfig->GetBool(kFilterSection, TEXT("bScene"),        FilterFlags.bScene,        Ini);
    GConfig->GetBool(kFilterSection, TEXT("bAllActors"),    FilterFlags.bAllActors,    Ini);
    GConfig->GetBool(kFilterSection, TEXT("bStaticMeshes"), FilterFlags.bStaticMeshes, Ini);
    GConfig->GetBool(kFilterSection, TEXT("bAllLights"),    FilterFlags.bAllLights,    Ini);
    GConfig->GetBool(kFilterSection, TEXT("bBlueprints"),   FilterFlags.bBlueprints,   Ini);
    GConfig->GetBool(kFilterSection, TEXT("bSequencer"),    FilterFlags.bSequencer,    Ini);
    GConfig->GetBool(kFilterSection, TEXT("bHDRI"),         FilterFlags.bHDRI,         Ini);
}
