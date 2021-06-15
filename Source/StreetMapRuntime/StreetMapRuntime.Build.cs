// Copyright 2017 Mike Fricker. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class StreetMapRuntime : ModuleRules
	{
        public StreetMapRuntime(ReadOnlyTargetRules Target)
			: base(Target)
		{
            PrivatePCHHeaderFile = "StreetMapRuntime.h";


            //@nsveri: removed "ShaderCore"
            PrivateDependencyModuleNames.AddRange(
                new string[] {
                    "Core",
					"CoreUObject",
					"Engine",
					"RHI",
					"RenderCore",
                    "PropertyEditor",
                    "Slate",
                    "SlateCore",
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