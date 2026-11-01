using UnrealBuildTool;

public class ScopePlugin : ModuleRules
{
    public ScopePlugin(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core", "CoreUObject", "Engine",
            // Slate types used in Public headers (SScopeShotPanel.h)
            "Slate", "SlateCore",
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            // Editor UI
            "UnrealEd", "EditorFramework",
            "ToolMenus", "LevelEditor", "InputCore", "AppFramework",
            // Camera
            "CinematicCamera",
            // Network / JSON
            "HTTP", "HTTPServer", "Json", "JsonUtilities",
            // Live Link
            "LiveLink", "LiveLinkInterface",
            // Rendering / Screenshot
            "RenderCore", "ImageWrapper",
            // Sequencer
            "LevelSequence", "MovieScene",
        });

        // nDisplay accessed via reflection — no hard link to avoid build failure when plugin absent
    }
}
