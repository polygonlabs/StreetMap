#include "StreetMapImporting.h"
#include "StreetMapComponent.h"
#include "StreetMapRailway.h"

#include "LandscapeProxy.h"
#include "LandscapeSplinesComponent.h"
#include "LandscapeSplineSegment.h"
#include "LandscapeSplineControlPoint.h"
#include "ControlPointMeshComponent.h"

#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "StreetMapImporting"

class StreetMapRailwayBuilder
{
public:
	ULandscapeSplinesComponent* CreateSplineComponent(ALandscapeProxy* Landscape, FVector Scale3D)
	{
		Landscape->Modify();
		Landscape->SplineComponent = NewObject<ULandscapeSplinesComponent>(Landscape, NAME_None, RF_Transactional);
		Landscape->SplineComponent->RelativeScale3D = Scale3D;
		Landscape->SplineComponent->AttachToComponent(Landscape->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		Landscape->SplineComponent->ShowSplineEditorMesh(true);

		return Landscape->SplineComponent;
	}

	void AddControlPoint(ULandscapeSplinesComponent* SplinesComponent, 
						 const FVector& LocalLocation,
						 const FStreetMapRailwayBuildSettings& BuildSettings,
						 ULandscapeSplineControlPoint* PreviousPoint = nullptr)
	{
		SplinesComponent->Modify();

		ULandscapeSplineControlPoint* NewControlPoint = NewObject<ULandscapeSplineControlPoint>(SplinesComponent, NAME_None, RF_Transactional);
		SplinesComponent->ControlPoints.Add(NewControlPoint);

		NewControlPoint->Location = LocalLocation;

		if (PreviousPoint)
		{
			NewControlPoint->Rotation = (NewControlPoint->Location - PreviousPoint->Location).Rotation();
			NewControlPoint->Width = PreviousPoint->Width;
			NewControlPoint->SideFalloff = PreviousPoint->SideFalloff;
			NewControlPoint->EndFalloff = PreviousPoint->EndFalloff;
			NewControlPoint->Mesh = PreviousPoint->Mesh;
			NewControlPoint->MeshScale = PreviousPoint->MeshScale;
			NewControlPoint->bPlaceSplineMeshesInStreamingLevels = PreviousPoint->bPlaceSplineMeshesInStreamingLevels;
			NewControlPoint->bEnableCollision = PreviousPoint->bEnableCollision;
			NewControlPoint->bCastShadow = PreviousPoint->bCastShadow;
		}
		else
		{
			// required to make control point visible
			NewControlPoint->UpdateSplinePoints();
		}

		if (!SplinesComponent->IsRegistered())
		{
			SplinesComponent->RegisterComponent();
		}
		else
		{
			SplinesComponent->MarkRenderStateDirty();
		}
	}

	void AddSegment(ULandscapeSplineControlPoint* Start, ULandscapeSplineControlPoint* End, bool bAutoRotateStart, bool bAutoRotateEnd)
	{
		FScopedTransaction Transaction(LOCTEXT("LandscapeSpline_AddSegment", "Add Landscape Spline Segment"));

		if (Start == End)
		{
			//UE_LOG( TEXT("Can't join spline control point to itself.") );
			return;
		}

		if (Start->GetOuterULandscapeSplinesComponent() != End->GetOuterULandscapeSplinesComponent())
		{
			//UE_LOG( TEXT("Can't join spline control points across different terrains.") );
			return;
		}

		for (const FLandscapeSplineConnection& Connection : Start->ConnectedSegments)
		{
			// if the *other* end on the connected segment connects to the "end" control point...
			if (Connection.GetFarConnection().ControlPoint == End)
			{
				//UE_LOG( TEXT("Spline control points already joined connected!") );
				return;
			}
		}

		ULandscapeSplinesComponent* SplinesComponent = Start->GetOuterULandscapeSplinesComponent();
		SplinesComponent->Modify();
		Start->Modify();
		End->Modify();

		ULandscapeSplineSegment* NewSegment = NewObject<ULandscapeSplineSegment>(SplinesComponent, NAME_None, RF_Transactional);
		SplinesComponent->Segments.Add(NewSegment);

		NewSegment->Connections[0].ControlPoint = Start;
		NewSegment->Connections[1].ControlPoint = End;

		NewSegment->Connections[0].SocketName = Start->GetBestConnectionTo(End->Location);
		NewSegment->Connections[1].SocketName = End->GetBestConnectionTo(Start->Location);

		FVector StartLocation; FRotator StartRotation;
		Start->GetConnectionLocationAndRotation(NewSegment->Connections[0].SocketName, StartLocation, StartRotation);
		FVector EndLocation; FRotator EndRotation;
		End->GetConnectionLocationAndRotation(NewSegment->Connections[1].SocketName, EndLocation, EndRotation);

		// Set up tangent lengths
		NewSegment->Connections[0].TangentLen = (EndLocation - StartLocation).Size();
		NewSegment->Connections[1].TangentLen = NewSegment->Connections[0].TangentLen;

		NewSegment->AutoFlipTangents();

		// set up other segment options
		ULandscapeSplineSegment* CopyFromSegment = nullptr;
		if (Start->ConnectedSegments.Num() > 0)
		{
			CopyFromSegment = Start->ConnectedSegments[0].Segment;
		}
		else if (End->ConnectedSegments.Num() > 0)
		{
			CopyFromSegment = End->ConnectedSegments[0].Segment;
		}
		else
		{
			// Use defaults
		}

		if (CopyFromSegment != nullptr)
		{
			NewSegment->LayerName = CopyFromSegment->LayerName;
			NewSegment->SplineMeshes = CopyFromSegment->SplineMeshes;
			NewSegment->LDMaxDrawDistance = CopyFromSegment->LDMaxDrawDistance;
			NewSegment->bRaiseTerrain = CopyFromSegment->bRaiseTerrain;
			NewSegment->bLowerTerrain = CopyFromSegment->bLowerTerrain;
			NewSegment->bPlaceSplineMeshesInStreamingLevels = CopyFromSegment->bPlaceSplineMeshesInStreamingLevels;
			NewSegment->bEnableCollision = CopyFromSegment->bEnableCollision;
			NewSegment->bCastShadow = CopyFromSegment->bCastShadow;
		}

		Start->ConnectedSegments.Add(FLandscapeSplineConnection(NewSegment, 0));
		End->ConnectedSegments.Add(FLandscapeSplineConnection(NewSegment, 1));

		bool bUpdatedStart = false;
		bool bUpdatedEnd = false;
		if (bAutoRotateStart)
		{
			Start->AutoCalcRotation();
			Start->UpdateSplinePoints();
			bUpdatedStart = true;
		}
		if (bAutoRotateEnd)
		{
			End->AutoCalcRotation();
			End->UpdateSplinePoints();
			bUpdatedEnd = true;
		}

		// Control points' points are currently based on connected segments, so need to be updated.
		if (!bUpdatedStart && Start->Mesh)
		{
			Start->UpdateSplinePoints();
		}
		if (!bUpdatedEnd && End->Mesh)
		{
			End->UpdateSplinePoints();
		}

		// If we've called UpdateSplinePoints on either control point it will already have called UpdateSplinePoints on the new segment
		if (!(bUpdatedStart || bUpdatedEnd))
		{
			NewSegment->UpdateSplinePoints();
		}
	}

	void Build(class UStreetMapComponent* StreetMapComponent, const FStreetMapRailwayBuildSettings& BuildSettings)
	{
		ULandscapeSplinesComponent* SplineComponent = CreateSplineComponent(BuildSettings.Landscape, FVector(1.0f));

		/*for ()
		{
			AddControlPoint()
		}*/

		/*for (ULandscapeSplineControlPoint* ControlPoint : SelectedSplineControlPoints)
		{
			//AddSegment(ControlPoint, NewControlPoint, bAutoRotateOnJoin, true);
		}*/
	}
};

void BuildRailway(class UStreetMapComponent* StreetMapComponent, const FStreetMapRailwayBuildSettings& BuildSettings)
{
	FScopedTransaction Transaction(LOCTEXT("Undo", "Creating Railways"));

	StreetMapRailwayBuilder Builder;

	Builder.Build(StreetMapComponent, BuildSettings);
}
