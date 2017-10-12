// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "StreetMapImporting.h"
#include "StreetMapCommands.h"

#define LOCTEXT_NAMESPACE "FStreetMapImporting"

void FStreetMapCommands::RegisterCommands()
{
	UI_COMMAND(SaveOsmFile, "Save OSM data", "Save to .osm file", EUserInterfaceActionType::Button, FInputGesture());
}

#undef LOCTEXT_NAMESPACE
