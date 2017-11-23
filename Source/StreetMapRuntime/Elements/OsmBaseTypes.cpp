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
	USceneComponent* Root = CreateDefaultSubobject<USplineComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Static);
	RootComponent = Root;
	WaySpline = CreateDefaultSubobject<USplineComponent>(TEXT("Way"));
	WaySpline->SetMobility(EComponentMobility::Static);
	RootComponent->SetMobility(EComponentMobility::Static);

	// sets a default to mesh to see something in the editor - has to be manually edited or overriden by child classes
	static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshAsset(TEXT("/Game/StarterContent/Shapes/Shape_Plane.Shape_Plane"));
	if (MeshAsset.Succeeded())
	{
		Mesh = MeshAsset.Object;
	}
}

void AOsmWay::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// if there is a mesh assigned, we can spawn a continous mesh along the spline
	if (Mesh)
	{
		// if there is a change in spline point size
		if (WaySpline->GetNumberOfSplinePoints() != SplineMeshComponents.Num() - 1 )
		{
			// if a spline point was added or deleted
			if (WaySpline->GetNumberOfSplinePoints() > SplineMeshComponents.Num() - 1)
			{
				// add missing nummber (just one normally as the construction script should run after each edit in editor)
				for (int32 NumToAdd = 0; NumToAdd < (WaySpline->GetNumberOfSplinePoints()-1 - SplineMeshComponents.Num()); NumToAdd++)
				{
					// TODO: name clash 
					USplineMeshComponent* SplineMesh = NewObject<USplineMeshComponent>(this);

					// using ConstructionScript would result in trashed objects! As we properly protect the NewObject call, this should be okay!
					SplineMesh->CreationMethod = EComponentCreationMethod::Instance; 
					SplineMesh->SetMobility(EComponentMobility::Static);
					SplineMesh->SetupAttachment(WaySpline);
					SplineMesh->SetStaticMesh(Mesh);

					SplineMesh->RegisterComponent();
					SplineMesh->UpdateRenderStateAndCollision();

					SplineMeshComponents.Add(SplineMesh);
				}
			}
			// remove components
			else
			{
				int32 NumToDestroy = SplineMeshComponents.Num() - (WaySpline->GetNumberOfSplinePoints() -1);
				for (int32 ComponentIndex = 0; ComponentIndex < NumToDestroy; ComponentIndex++)
				{
					SplineMeshComponents[ComponentIndex]->UnregisterComponent();
					SplineMeshComponents[ComponentIndex]->DestroyComponent(true);
					SplineMeshComponents.RemoveAt(ComponentIndex);
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
}