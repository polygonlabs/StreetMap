// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "StreetMapImporting.h"
#include "StreetMapCommands.h"

#define LOCTEXT_NAMESPACE "FStreetMapImporting"

void FStreetMapCommands::RegisterCommands()
{
	UI_COMMAND(PluginAction, "StreetMapToolbar", "Execute StreetMapToolbar action", EUserInterfaceActionType::Button, FInputGesture());
}

#undef LOCTEXT_NAMESPACE
