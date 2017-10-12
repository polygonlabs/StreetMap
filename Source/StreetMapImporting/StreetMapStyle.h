// Copyright 2017 Mike Fricker. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"

/** StreetMap Editor Style Helper Class. */
class FStreetMapStyle
{
public:

	static void Initialize();
	static void Shutdown();

	/** reloads textures used by slate renderer */
	static void ReloadTextures();

	static TSharedPtr< class ISlateStyle > Get();

	static FName GetStyleSetName();

private:

	static FString InContent(const FString& RelativePath, const ANSICHAR* Extension);
	static TSharedRef< class FSlateStyleSet > Create();

private:

	static TSharedPtr< class FSlateStyleSet > StyleSet;
};