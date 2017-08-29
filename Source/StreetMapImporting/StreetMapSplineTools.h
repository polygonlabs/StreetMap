#pragma once

class FStreetMapSplineTools
{
public:
	static ULandscapeSplinesComponent* ConditionallyCreateSplineComponent(ALandscapeProxy* Landscape, FVector Scale3D);
	static ULandscapeSplineControlPoint* AddControlPoint(ULandscapeSplinesComponent* SplinesComponent,
			const FVector& LocalLocation,
			const float& Width,
			const float& ZOffset,
			const ALandscapeProxy* Landscape,
			ULandscapeSplineControlPoint* PreviousPoint = nullptr);

	static ULandscapeSplineSegment* AddSegment(ULandscapeSplineControlPoint* Start, ULandscapeSplineControlPoint* End, bool bAutoRotateStart, bool bAutoRotateEnd);
	
	static float GetLandscapeElevation(const ALandscapeProxy* Landscape, const FVector2D& Location);

	static void CleanSplines(ULandscapeSplinesComponent* SplinesComponent,
		const UStaticMesh* Mesh,
		UWorld* World);
};
