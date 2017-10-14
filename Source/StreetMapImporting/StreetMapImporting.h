// Copyright 2017 Mike Fricker. All Rights Reserved.
#pragma once

#include "UnrealEd.h"
#include "CoreMinimal.h"
#include "ModuleManager.h"
#include "StreetMapAssetTypeActions.h"
#include "OSMFile.h"
#include "Elements/OsmBaseTypes.h"

class FToolBarBuilder;
class FMenuBuilder;

class FStreetMapImportingModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** This functions will be bound to Command. */
	void SaveOsmButtonClicked();
	void LoadOsmButtonClicked();
private:

	void AddToolbarExtension(FToolBarBuilder& Builder);
	void AddMenuExtension(FMenuBuilder& Builder);

	void ShowErrorMessage(const FText& MessageText);
	void ShowInfoMessage(const FText& MessageText);

	AOsmNode* GetSpecializedActorFromNodeTags(UWorld* World, UOSMFile::FOSMNodeInfo* Node, FVector Location, FRotator Rotation, FActorSpawnParameters SpawnInfo);

private:
	TSharedPtr<class FUICommandList> PluginCommands;
	TSharedPtr< FStreetMapAssetTypeActions > StreetMapAssetTypeActions;
};