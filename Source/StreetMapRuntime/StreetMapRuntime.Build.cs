// Copyright 2017 Mike Fricker. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class StreetMapRuntime : ModuleRules
	{
        public StreetMapRuntime(ReadOnlyTargetRules Target) : base(Target)
        {
            PrivatePCHHeaderFile = "StreetMapRuntime.h";
            PrivateDependencyModuleNames.AddRange(
                new string[] {
                    "Core",
                    "CoreUObject",
                    "Engine",
                    "RHI",
                    "RenderCore",
                    "Renderer",
                    "Landscape"
                }
            );

            PublicDependencyModuleNames.AddRange(
                new string[]
                {
                    "GeometricObjects",
                    "ProceduralMeshComponent"
                }
            );

            if (Target.bBuildEditor == true)
            {
                PrivateDependencyModuleNames.Add("PropertyEditor");
            }
        }
	}
}