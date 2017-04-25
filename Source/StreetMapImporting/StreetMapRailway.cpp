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

void CreateSplineComponent(ALandscapeProxy* Landscape, FVector Scale3D)
{
	Landscape->Modify();
	Landscape->SplineComponent = NewObject<ULandscapeSplinesComponent>(Landscape, NAME_None, RF_Transactional);
	Landscape->SplineComponent->RelativeScale3D = Scale3D;
	Landscape->SplineComponent->AttachToComponent(Landscape->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	Landscape->SplineComponent->ShowSplineEditorMesh(true);
}


void AddControlPoint(ULandscapeSplinesComponent* SplinesComponent, const FVector& LocalLocation)
{
/*	SplinesComponent->Modify();

	ULandscapeSplineControlPoint* NewControlPoint = NewObject<ULandscapeSplineControlPoint>(SplinesComponent, NAME_None, RF_Transactional);
	SplinesComponent->ControlPoints.Add(NewControlPoint);

	NewControlPoint->Location = LocalLocation;

	if (SelectedSplineControlPoints.Num() > 0)
	{
		ULandscapeSplineControlPoint* FirstPoint = *SelectedSplineControlPoints.CreateConstIterator();
		NewControlPoint->Rotation = (NewControlPoint->Location - FirstPoint->Location).Rotation();
		NewControlPoint->Width = FirstPoint->Width;
		NewControlPoint->SideFalloff = FirstPoint->SideFalloff;
		NewControlPoint->EndFalloff = FirstPoint->EndFalloff;
		NewControlPoint->Mesh = FirstPoint->Mesh;
		NewControlPoint->MeshScale = FirstPoint->MeshScale;
		NewControlPoint->bPlaceSplineMeshesInStreamingLevels = FirstPoint->bPlaceSplineMeshesInStreamingLevels;
		NewControlPoint->bEnableCollision = FirstPoint->bEnableCollision;
		NewControlPoint->bCastShadow = FirstPoint->bCastShadow;

		for (ULandscapeSplineControlPoint* ControlPoint : SelectedSplineControlPoints)
		{
			//AddSegment(ControlPoint, NewControlPoint, bAutoRotateOnJoin, true);
		}
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
	}*/
}


void BuildRailway(class UStreetMapComponent* StreetMapComponent, const FStreetMapRailwayBuildSettings& BuildSettings)
{
	FScopedTransaction Transaction(LOCTEXT("Undo", "Creating Railways"));



}
