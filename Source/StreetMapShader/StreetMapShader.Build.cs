// Copyright 2017 Mike Fricker. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class StreetMapShader : ModuleRules
	{
        public StreetMapShader(ReadOnlyTargetRules Target) : base(Target)
        {
            PrivatePCHHeaderFile = "StreetMapShader.h";
            PrivateDependencyModuleNames.AddRange(
				new string[] {
                    "Core",
					"CoreUObject",
					"Engine",
					"RHI",
					"RenderCore",
					"Renderer"
                }
			);

            if (Target.bBuildEditor == true)
            {
                PrivateDependencyModuleNames.Add("PropertyEditor");
            }
        }
	}
}