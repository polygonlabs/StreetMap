// Copyright 2017 Mike Fricker. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class StreetMapImporting : ModuleRules
    {
        public StreetMapImporting(ReadOnlyTargetRules Target) : base(Target)
        {
            bEnforceIWYU = false;

            PrivatePCHHeaderFile = "StreetMapImporting.h";
            PrivateDependencyModuleNames.AddRange(
                new string[] {
                    "Core",
                    "CoreUObject",
                    "Engine",
                    "XmlParser",
                    "AssetTools",
                    "Projects",
                    "Slate",
                    "EditorStyle",
                    "SlateCore",
                    "UnrealEd",
                    "PropertyEditor",
                    "RenderCore",
                    "RHI",
                    "RawMesh",
                    "AssetTools",
                    "AssetRegistry",
                    "StreetMapRuntime",
                    "HTTP",
                    "ImageWrapper",
                    "DesktopPlatform",
                    "Landscape",
                    "CinematicCamera"
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