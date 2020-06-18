// Copyright 2017 Mike Fricker. All Rights Reserved.


#include "StreetMap.h"
#include <math.h>
#include "StreetMapRuntime.h"
#include "EditorFramework/AssetImportData.h"

UStreetMap::UStreetMap()
{
#if WITH_EDITORONLY_DATA
	if( !HasAnyFlags( RF_ClassDefaultObject ) )
	{
		AssetImportData = NewObject<UAssetImportData>( this, TEXT( "AssetImportData" ) );
	}
#endif
}

void UStreetMap::GetAssetRegistryTags( TArray<FAssetRegistryTag>& OutTags ) const
{
#if WITH_EDITORONLY_DATA
	if( AssetImportData )
	{
		OutTags.Add( FAssetRegistryTag( SourceFileTagName(), AssetImportData->GetSourceData().ToJson(), FAssetRegistryTag::TT_Hidden ) );
	}
#endif

	Super::GetAssetRegistryTags( OutTags );
}


void FStreetMapRoad::ComputeUVspan(float startV
	, float Thickness
)
{
	textureVStart.X = startV;
	float VAccumulation = textureVStart.X;
	// add length of each segment
	for (int i = 0; i < (RoadPoints.Num() - 1); ++i)
	{
		auto Point1 = RoadPoints[i];
		auto Point2 = RoadPoints[i + 1];

		const float distance = (Point2 - Point1).Size();
		const float xRatio = (distance / Thickness);
		VAccumulation += xRatio;
	}
	double intpart;
	double frac = modf(VAccumulation, &intpart);
	textureVStart.Y = frac;

	lengthComputed = true;
}

void FStreetMapRoad::ComputeUVspanFromBack(float endV
	, float Thickness
)
{
	ComputeUVspan(0.f, Thickness);
	double intpart;
	double frac = modf(textureVStart.Y, &intpart);
	auto diff = endV - frac;

	textureVStart.Y = textureVStart.Y + diff;
	textureVStart.X = textureVStart.X + diff;

	while (textureVStart.X < 0.f)
	{
		textureVStart.X += 1.f;
		textureVStart.Y += 1.f;
	}
}