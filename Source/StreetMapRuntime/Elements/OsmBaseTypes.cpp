// Fill out your copyright notice in the Description page of Project Settings.

#include "StreetMapRuntime.h"
#include "OsmBaseTypes.h"


AOsmNode::AOsmNode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = MeshComponent;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshAsset(TEXT("/Game/StarterContent/Shapes/Shape_Plane.Shape_Plane"));
	if (MeshAsset.Succeeded())
	{
		MeshComponent->SetStaticMesh(MeshAsset.Object);
	}
}



AOsmWay::AOsmWay(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	WaySpline = CreateDefaultSubobject<USplineComponent>(TEXT("Way"));
	RootComponent = WaySpline;

	// sets a default to mesh to see something in the editor - has to be manually edited or overriden by child classes
	static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshAsset(TEXT("/Game/StarterContent/Shapes/Shape_Plane.Shape_Plane"));
	if (MeshAsset.Succeeded())
	{
		Mesh = MeshAsset.Object;
	}
}

void AOsmWay::OnConstruction(const FTransform& Transform)
{
	// if there is a mesh assigned, we can spawn a continous mesh along the spline
	if (Mesh)
	{
		// if a spline point was added or deleted
		if (WaySpline->GetNumberOfSplinePoints()-1 != SplineMeshComponents.Num())
		{
			if (WaySpline->GetNumberOfSplinePoints()-1 > SplineMeshComponents.Num())
			{
				// add missing nummber (just one always?)
				for (int32 NumToAdd = 0; NumToAdd < (WaySpline->GetNumberOfSplinePoints() - SplineMeshComponents.Num()); NumToAdd++)
				{
					USplineMeshComponent* SplineMesh = NewObject<USplineMeshComponent>(this);
					SplineMeshComponents.Add(SplineMesh);

					SplineMesh->CreationMethod = EComponentCreationMethod::UserConstructionScript;
					SplineMesh->SetMobility(EComponentMobility::Movable);
					SplineMesh->AttachToComponent(WaySpline, FAttachmentTransformRules(EAttachmentRule::KeepRelative, true));
					SplineMesh->bCastDynamicShadow = false;
					SplineMesh->SetStaticMesh(Mesh);

					//Set the color!
					//UMaterialInstanceDynamic* dynamicMat = UMaterialInstanceDynamic::Create(mSplineMeshMaterial, NULL);
					//dynamicMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(mSegments[i].mColor));

					//SplineMesh->SetMaterial(0, dynamicMat);

					//Width of the mesh 
					SplineMesh->SetStartScale(FVector2D(1, 1));
					SplineMesh->SetEndScale(FVector2D(1, 1));
				}
				RegisterAllComponents();
			}
			else
			{
				// remove components
				int32 NumToDestroy = SplineMeshComponents.Num() - (WaySpline->GetNumberOfSplinePoints() -1);
				for (int32 ComponentIndex = 0; ComponentIndex < NumToDestroy; ComponentIndex++)
				{
					SplineMeshComponents.Last()->DestroyComponent(true);
				}
			}
		}


		// now recreate the splinemesh
		for (int32 PointIndex = 0; PointIndex < WaySpline->GetNumberOfSplinePoints() - 1; PointIndex++)
		{
			FVector PointLocationStart, PointTangentStart, PointLocationEnd, PointTangentEnd;
			WaySpline->GetLocalLocationAndTangentAtSplinePoint(PointIndex, PointLocationStart, PointTangentStart);
			WaySpline->GetLocalLocationAndTangentAtSplinePoint(PointIndex + 1, PointLocationEnd, PointTangentEnd);

			SplineMeshComponents[PointIndex]->SetStartAndEnd(PointLocationStart, PointTangentStart, PointLocationEnd, PointTangentEnd);
		}
	}
}