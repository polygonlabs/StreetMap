// Copyright 2017 Mike Fricker. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class StreetMapImporting : ModuleRules
    {
        public StreetMapImporting(ReadOnlyTargetRules Target) : base(Target)
        {
            // PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

            PrivateDependencyModuleNames.AddRange(
                new string[] {
                    "Core",
                    "CoreUObject",
                    "Engine",
                    "UnrealEd",
                    "XmlParser",
                    "AssetTools",
                    "Projects",
                    "Slate",
                    "EditorStyle",
                    "SlateCore",
                    "PropertyEditor",
                    "RenderCore",
                    "ShaderCore",
                    "RHI",
                    "RawMesh",
                    "AssetTools",
                    "AssetRegistry",
                    "StreetMapRuntime",
                    "HTTP",
                    "ImageWrapper",
                    "DesktopPlatform",
                    "Landscape",
                    "CinematicCamera",
                    "InputCore",
                    "LevelEditor",
                }
            );

            PrivateIncludePaths.AddRange(
                new string[] {
                    "Developer/DesktopPlatform/Public",
                }
            );
        }
    }
}