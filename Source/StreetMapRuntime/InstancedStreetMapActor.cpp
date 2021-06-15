
#include "InstancedStreetMapActor.h"
#include "StreetMap.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"

AInstancedStreetMapActor::AInstancedStreetMapActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer),
	ScaleFactor(0.1f)
{
	InstancedMeshComponent = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Instanced Mesh Component"));
}

void AInstancedStreetMapActor::BuildStreetMap()
{
	InstancedMeshComponent->ClearInstances();

	if (StreetMap != nullptr)
	{	
		auto& Roads = StreetMap->GetRoads();

		for (auto& Road : Roads)
		{
			FTransform SegmentTransform;

			for (int i=0; i<Road.RoadPoints.Num(); i++)
			{
				SegmentTransform.SetLocation(FVector(Road.RoadPoints[i],0.0f) * ScaleFactor);

				InstancedMeshComponent->AddInstance(SegmentTransform);
			}
			
		}

	}
}
