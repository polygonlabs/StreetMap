// Copyright 2017 Mike Fricker. All Rights Reserved.

#include "StreetMapImporting.h"
#include "StreetMapAssetTypeActions.h"
#include "ModuleManager.h"
#include "StreetMapStyle.h"
#include "StreetMapComponentDetails.h"
#include "StreetMapCommands.h"
#include "Misc/MessageDialog.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#include "LevelEditor.h"


IMPLEMENT_MODULE( FStreetMapImportingModule, StreetMapImporting )

#define LOCTEXT_NAMESPACE "FStreetMapImporting"


void FStreetMapImportingModule::StartupModule()
{
	// Register asset types
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>( "AssetTools" ).Get();
	StreetMapAssetTypeActions = MakeShareable( new FStreetMapAssetTypeActions() );
	AssetTools.RegisterAssetTypeActions( StreetMapAssetTypeActions.ToSharedRef() );

	// Initialize & Register StreetMap Style
	FStreetMapStyle::Initialize();
	FStreetMapStyle::ReloadTextures();

	// Register StreetMapComponent Detail Customization
	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout("StreetMapComponent", FOnGetDetailCustomizationInstance::CreateStatic(&FStreetMapComponentDetails::MakeInstance));
	PropertyModule.NotifyCustomizationModuleChanged();

	// Registering Visualizer
	if (GUnrealEd != NULL)
	{
		TSharedPtr<FComponentVisualizer> Visualizer = MakeShareable(new FStreetMapComponentVisualizer());

		if (Visualizer.IsValid())
		{
			GUnrealEd->RegisterComponentVisualizer(UStreetMapComponent::StaticClass()->GetFName(), Visualizer);
			Visualizer->OnRegister();
		}

	}

	// Toolbar plugin code
	FStreetMapCommands::Register();

	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FStreetMapCommands::Get().PluginAction,
		FExecuteAction::CreateRaw(this, &FStreetMapImportingModule::PluginButtonClicked),
		FCanExecuteAction());

	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");

	{
		TSharedPtr<FExtender> MenuExtender = MakeShareable(new FExtender());
		MenuExtender->AddMenuExtension("WindowLayout", EExtensionHook::First, PluginCommands, FMenuExtensionDelegate::CreateRaw(this, &FStreetMapImportingModule::AddMenuExtension));

		LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MenuExtender);
	}

	{
		TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);
		ToolbarExtender->AddToolBarExtension("Settings", EExtensionHook::First, PluginCommands, FToolBarExtensionDelegate::CreateRaw(this, &FStreetMapImportingModule::AddToolbarExtension));

		LevelEditorModule.GetToolBarExtensibilityManager()->AddExtender(ToolbarExtender);
	}
}


void FStreetMapImportingModule::ShutdownModule()
{
	// Unregister all the asset types that we registered
	if( FModuleManager::Get().IsModuleLoaded( "AssetTools" ) )
	{
		IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>( "AssetTools" ).Get();
		AssetTools.UnregisterAssetTypeActions( StreetMapAssetTypeActions.ToSharedRef() );
		StreetMapAssetTypeActions.Reset();
	}

	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomClassLayout("StreetMapComponent");
		PropertyModule.NotifyCustomizationModuleChanged();
	}

	// Unregister Visualizer
	if (GUnrealEd != NULL)
	{
		GUnrealEd->UnregisterComponentVisualizer(UStreetMapComponent::StaticClass()->GetFName());
	}

	// Unregister StreetMap Style
	FStreetMapStyle::Shutdown();

	FStreetMapCommands::Unregister();
}

void FStreetMapImportingModule::PluginButtonClicked()
{
	// Put your "OnButtonClicked" stuff here
	FText DialogText = FText::Format(
		LOCTEXT("PluginButtonDialogText", "Add code to {0} in {1} to override this button's actions"),
		FText::FromString(TEXT("FStreetMapToolbarModule::PluginButtonClicked()")),
		FText::FromString(TEXT("StreetMapToolbar.cpp"))
	);
	FMessageDialog::Open(EAppMsgType::Ok, DialogText);
}

void FStreetMapImportingModule::AddMenuExtension(FMenuBuilder& Builder)
{
	Builder.AddMenuEntry(FStreetMapCommands::Get().PluginAction);
}

void FStreetMapImportingModule::AddToolbarExtension(FToolBarBuilder& Builder)
{
	Builder.AddToolBarButton(FStreetMapCommands::Get().PluginAction);
}

#undef LOCTEXT_NAMESPACE