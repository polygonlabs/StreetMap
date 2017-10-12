// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "StreetMapStyle.h"

class FStreetMapCommands : public TCommands<FStreetMapCommands>
{
public:

	FStreetMapCommands()
		: TCommands<FStreetMapCommands>(TEXT("StreetMapEditor"), NSLOCTEXT("Contexts", "StreetMapEditor", "StreetMapEditor Plugin"), NAME_None, FStreetMapStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > SaveOsmFile;
};
