// ScopePluginModule.cpp
// Registers the NomadTab spawner, Window menu entry, and toolbar button.

#include "ScopePluginModule.h"
#include "SScopeShotPanel.h"
#include "ScopeRestoreServer.h"
#include "Framework/Docking/TabManager.h"
#include "ToolMenus.h"
#include "LevelEditor.h"
#include "Widgets/Docking/SDockTab.h"
#include "Styling/AppStyle.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "FScopePluginModule"

static const FName ScopeTabName(TEXT("ScopePlugin"));

// ---------------------------------------------------------------------------
// Module lifecycle
// ---------------------------------------------------------------------------
void FScopePluginModule::StartupModule()
{
    // Register NomadTab — omit SetMenuType(Hidden) so it appears in Window menu.
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        ScopeTabName,
        FOnSpawnTab::CreateLambda([](const FSpawnTabArgs&) -> TSharedRef<SDockTab>
        {
            return SNew(SDockTab)
                .TabRole(ETabRole::NomadTab)
                [
                    SNew(SScopeShotPanel)
                ];
        }))
        .SetDisplayName(LOCTEXT("TabTitle",   "Scope Shot"))
        .SetTooltipText(LOCTEXT("TabTooltip", "Open the Scope Shot panel"));

    UToolMenus::RegisterStartupCallback(
        FSimpleMulticastDelegate::FDelegate::CreateRaw(
            this, &FScopePluginModule::RegisterMenuExtensions));

    // Scene Restore HTTP listener — port from Config/ScopePlugin.ini (default 30020).
    int32 RestorePort = 30020;
    const FString IniPath = FPaths::ProjectConfigDir() / TEXT("ScopePlugin.ini");
    GConfig->GetInt(TEXT("ScopePlugin"), TEXT("RestorePort"), RestorePort, IniPath);
    FScopeRestoreServer::Start(static_cast<uint32>(RestorePort));
}

void FScopePluginModule::ShutdownModule()
{
    FScopeRestoreServer::Stop();
    UToolMenus::UnRegisterStartupCallback(this);
    UToolMenus::UnregisterOwner(this);
    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ScopeTabName);
}

// ---------------------------------------------------------------------------
// Menu / toolbar registration
// ---------------------------------------------------------------------------
void FScopePluginModule::RegisterMenuExtensions()
{
    FToolMenuOwnerScoped OwnerScoped(this);

    // ── Window menu entry ────────────────────────────────────────────────────
    // "MainFrame.MainMenu.Window" is the standard UE5 Window menu.
    UToolMenu* WindowMenu = UToolMenus::Get()->ExtendMenu("MainFrame.MainMenu.Window");
    if (WindowMenu)
    {
        FToolMenuSection& WinSection = WindowMenu->FindOrAddSection(
            TEXT("ScopePlugin"),
            LOCTEXT("WindowSectionLabel", "Scope"));

        WinSection.AddMenuEntry(
            TEXT("OpenScopeShot"),
            LOCTEXT("WindowMenuLabel",   "Scope Shot"),
            LOCTEXT("WindowMenuTooltip", "Open the Scope Shot panel"),
            FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"),
            FUIAction(FExecuteAction::CreateLambda([]()
            {
                FGlobalTabmanager::Get()->TryInvokeTab(FTabId(ScopeTabName));
            }))
        );
    }

    // ── Toolbar button ───────────────────────────────────────────────────────
    UToolMenu* Toolbar =
        UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
    if (!Toolbar)
        Toolbar = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar");
    if (!Toolbar) return;

    FToolMenuSection& ToolbarSection = Toolbar->FindOrAddSection(TEXT("ScopePlugin"));
    ToolbarSection.AddEntry(FToolMenuEntry::InitToolBarButton(
        TEXT("OpenScopeShot"),
        FUIAction(FExecuteAction::CreateLambda([]()
        {
            FGlobalTabmanager::Get()->TryInvokeTab(FTabId(ScopeTabName));
        })),
        LOCTEXT("ToolbarLabel",   "Scope Shot"),
        LOCTEXT("ToolbarTooltip", "Open the Scope Shot panel"),
        FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details")
    ));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FScopePluginModule, ScopePlugin)
