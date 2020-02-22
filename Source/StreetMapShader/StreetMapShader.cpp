// Copyright 2017 Mike Fricker. All Rights Reserved.

#include "StreetMapShader.h"
#include "Modules/ModuleManager.h"


class FStreetMapShaderModule : public IModuleInterface
{

public:

	// IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

};


IMPLEMENT_MODULE(FStreetMapShaderModule, StreetMapShader)



void FStreetMapShaderModule::StartupModule()
{
	FString ShaderDirectory = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("StreetMap/Shaders"));
	AddShaderSourceDirectoryMapping("/Plugin", ShaderDirectory);
}


void FStreetMapShaderModule::ShutdownModule()
{
}

