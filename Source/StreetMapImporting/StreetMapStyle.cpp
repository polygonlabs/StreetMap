// Copyright 2017 Mike Fricker. All Rights Reserved.

#include "StreetMapImporting.h"
#include "StreetMapStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/SlateStyleRegistry.h"
#include "SlateGameResources.h"
#include "SlateStyle.h"
#include "IPluginManager.h"

TSharedPtr< FSlateStyleSet > FStreetMapStyle::StyleSet = NULL;
TSharedPtr< class ISlateStyle > FStreetMapStyle::Get() { return StyleSet; }

#define IMAGE_BRUSH( RelativePath, ... ) FSlateImageBrush( StyleSet->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define BOX_BRUSH( RelativePath, ... ) FSlateBoxBrush( StyleSet->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define BORDER_BRUSH( RelativePath, ... ) FSlateBorderBrush( StyleSet->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define TTF_FONT( RelativePath, ... ) FSlateFontInfo( StyleSet->RootToContentDir( RelativePath, TEXT(".ttf") ), __VA_ARGS__ )
#define OTF_FONT( RelativePath, ... ) FSlateFontInfo( StyleSet->RootToContentDir( RelativePath, TEXT(".otf") ), __VA_ARGS__ )

FString FStreetMapStyle::InContent(const FString& RelativePath, const ANSICHAR* Extension)
{
	static FString IconsDir = IPluginManager::Get().FindPlugin(TEXT("StreetMap"))->GetContentDir() / TEXT("Icons");
	return (IconsDir / RelativePath) + Extension;
}

void FStreetMapStyle::Initialize()
{
	// Const icon & thumbnail sizes
	const FVector2D Icon16x16(16.0f, 16.0f);
	const FVector2D Icon20x20(20.0f, 20.0f);
	const FVector2D Icon40x40(40.0f, 40.0f);
	const FVector2D Icon128x128(128.0f, 128.0f);

	// Only register once
	if (StyleSet.IsValid())
	{
		return;
	}

	StyleSet = MakeShareable(new FSlateStyleSet(GetStyleSetName()));
	StyleSet->SetContentRoot(IPluginManager::Get().FindPlugin("StreetMap")->GetBaseDir() / TEXT("Resources"));

	StyleSet->Set("ClassIcon.StreetMap", new IMAGE_BRUSH("sm_asset_icon_32", Icon16x16));
	StyleSet->Set("ClassThumbnail.StreetMap", new IMAGE_BRUSH("sm_asset_icon_128", Icon128x128));

	StyleSet->Set("ClassIcon.StreetMapActor", new IMAGE_BRUSH("sm_actor_icon_32", Icon16x16));
	StyleSet->Set("ClassThumbnail.StreetMapActor", new IMAGE_BRUSH("sm_actor_icon_128", Icon128x128));

	StyleSet->Set("ClassIcon.StreetMapComponent", new IMAGE_BRUSH("sm_component_icon_32", Icon16x16));
	StyleSet->Set("ClassThumbnail.StreetMapComponent", new IMAGE_BRUSH("sm_component_icon_128", Icon128x128));

	StyleSet->Set("StreetMapEditor.SaveOsmFile", new IMAGE_BRUSH(TEXT("Icon128_Save"), Icon40x40));
	StyleSet->Set("StreetMapEditor.LoadOsmFile", new IMAGE_BRUSH(TEXT("Icon128_Load"), Icon40x40));

	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet.Get());
};

#undef IMAGE_BRUSH
#undef BOX_BRUSH
#undef BORDER_BRUSH
#undef TTF_FONT
#undef OTF_FONT

void FStreetMapStyle::Shutdown()
{
	if (StyleSet.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet.Get());
		ensure(StyleSet.IsUnique());
		StyleSet.Reset();
	}
}

FName FStreetMapStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("StreetMapStyle"));
	return StyleSetName;
}

void FStreetMapStyle::ReloadTextures()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
	}
}