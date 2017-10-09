#include "StreetMapImporting.h"
#include "StreetMapActor.h"
#include "StreetMapComponent.h"
#include "Elements/TrafficSign.h"
#include "Elements/PowerGeneratorWind.h"

#include "StreetMapSplineTools.h"

#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "StreetMapImporting"

static void BuildStreetMapRoadFurniture(class UStreetMapComponent* StreetMapComponent,
	const FStreetMapRoadFurnitureBuildSettings& BuildSettings)
{
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (World)
	{
		// Traffic Signs
		const TArray<FStreetMapSign>& Signs = StreetMapComponent->GetStreetMap()->GetSigns();
		for (auto Sign : Signs)
		{
			const float WorldElevation = FStreetMapSplineTools::GetLandscapeElevation(BuildSettings.Landscape, Sign.Location);
			
			FRotator Rotation(0.0f, 0.0f, 0.0f);
			FActorSpawnParameters SpawnInfo;
			SpawnInfo.Name = FName(*Sign.NodeId);
			auto NewActor = World->SpawnActor<ATrafficSign>(FVector(Sign.Location, WorldElevation), Rotation, SpawnInfo);
			if (NewActor)
			{
				NewActor->SetFolderPath("Nodes/Signs");
				NewActor->SetActorLabel(SpawnInfo.Name.ToString());
			}
		}

		// Wind Turbines
		const TArray<FStreetMapWindTurbine>& WindTurbines = StreetMapComponent->GetStreetMap()->GetWindTurbines();
		for (auto WindTurbine : WindTurbines)
		{
			const float WorldElevation = FStreetMapSplineTools::GetLandscapeElevation(BuildSettings.Landscape, WindTurbine.Location);

			FRotator Rotation(0.0f, 0.0f, 0.0f);
			FActorSpawnParameters SpawnInfo;
			SpawnInfo.Name = FName(*WindTurbine.NodeId);
			auto NewActor = World->SpawnActor<APowerGeneratorWind>(FVector(WindTurbine.Location, WorldElevation), Rotation, SpawnInfo);
			if (NewActor)
			{
				NewActor->SetFolderPath("Nodes/WindTurbines");
				NewActor->SetActorLabel(SpawnInfo.Name.ToString());
			}
		}
	}
}


void BuildRoadFurniture(class UStreetMapComponent* StreetMapComponent, const FStreetMapRoadFurnitureBuildSettings& BuildSettings)
{
	FScopedTransaction Transaction(LOCTEXT("Undo", "Creating Road Furniture"));

	BuildStreetMapRoadFurniture(StreetMapComponent, BuildSettings);
}
