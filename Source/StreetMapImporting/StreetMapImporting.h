// Copyright 2017 Mike Fricker. All Rights Reserved.
#pragma once

#include "UnrealEd.h"
#include "CoreMinimal.h"
#include "ModuleManager.h"
#include "StreetMapAssetTypeActions.h"

class FToolBarBuilder;
class FMenuBuilder;

class FStreetMapImportingModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** This function will be bound to Command. */
	void PluginButtonClicked();

private:

	void AddToolbarExtension(FToolBarBuilder& Builder);
	void AddMenuExtension(FMenuBuilder& Builder);

private:
	TSharedPtr<class FUICommandList> PluginCommands;
	TSharedPtr< FStreetMapAssetTypeActions > StreetMapAssetTypeActions;
};